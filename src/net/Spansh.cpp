//
// Created by mkizub on 10.07.2026.
//
#include "../pch.h"

#include "Spansh.h"
#include "HttpInterceptor.h"
#include "../Galaxy.h"
#include "../db/DB.h"

#include <curl/curl.h>
#include <cpr/cpr.h>

const std::string API = "https://spansh.co.uk/api/";

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif

static void parseBodyId(const gal::spStarSystem& ss, gal::spEntity& entity, const js::value& j) {
    if (j.at("bodyId").is_int())
        entity->bodyId = j["bodyId"].as_int();
    if (j["parents"].is_array() && !j["parents"].as_array().empty()) {
        auto& jp = j["parents"].as_array()[0];
        if (jp.is_object())
            entity->parentBodyId = jp.key_value().begin().value().as_int();
        auto b = entity;
        for (const auto& jp : j["parents"].as_array()) {
            TypeNav p_type = TypeNav::Error;
            int p_id = -1;
            if (jp["Null"].is_int()) {
                p_type = TypeNav::Barycenter;
                p_id = jp["Null"].as_int();
            }
            else if (jp["Star"].is_int()) {
                p_type = TypeNav::Star;
                p_id = jp["Star"].as_int();
            }
            else if (jp["Planet"].is_int()) {
                p_type = TypeNav::Planet;
                p_id = jp["Planet"].as_int();
            }
            else if (jp["Ring"].is_int()) {
                p_type = TypeNav::Ring;
                p_id = jp["Ring"].as_int();
            }
            else if (jp["AsteroidCluster"].is_int()) {
                p_type = TypeNav::AsteroidCluster;
                p_id = jp["AsteroidCluster"].as_int();
            }
            if (p_type == TypeNav::Error || p_id < 0)
                break;
            if (b->parentBodyId != p_id) {
                b->parentBodyId = p_id;
                ss->saved = false;
            }
            auto p = ss->getBodyById(p_id);
            if (!p) {
                p = std::make_shared<gal::Entity>();
                p->type = p_type;
                p->bodyId = p_id;
                ss->bodies.push_back(p);
                ss->saved = false;
            }
            b = p;
        }
    }
}

static bool loadMarket(int64_t marketId, std::string stationName, std::string stationType, std::string starSystem, const js::value& jm) {
    if (!marketId)
        return false;

    Timestamp timestamp;
    if (!parseTimestamp(jm["updateTime"], timestamp))
        return false;
    spMarket old_market = gal::getMarket(marketId);
    if (old_market && old_market->timestamp >= timestamp)
        return false;
    if (old_market && !old_market->stationType.empty())
        stationType = old_market->stationType;

    spMarket market = std::make_shared<Market>(Market{
            .timestamp = timestamp,
            .marketId = marketId,
            .stationName = stationName,
            .stationType = stationType,
            .starSystem = starSystem,
    });
    if (old_market)
        market->raven = old_market->raven;

    auto& items = jm["commodities"].as_array_or();
    for (auto it : items) {
        Commodity* commodity = Cfg.getCommodityById(toLower(it["symbol"].as_string()));
        if (!commodity)
            continue;
        MarketLine ml {};
        ml.buyPrice = it["buyPrice"].as_int_or();
        ml.sellPrice = it["sellPrice"].as_int_or();
        ml.stock = it["supply"].as_int_or();
        ml.demand = it["demand"].as_int_or();
        if (old_market && old_market->items.contains(commodity)) {
            ml.isConsumer = market->items[commodity].isConsumer;
            ml.isProducer = market->items[commodity].isProducer;
        }
        market->items.emplace(commodity, ml);
    }
    gal::setMarketData(market);

    return true;
}

static void parseStation(const gal::spStarSystem& ss, const js::value& jb, int parentBodyId=-1) {
    bool is_new = true;
    auto marketId = jb["id"].as_int_or();
    auto name = jb["name"].as_string_or();
    gal::spEntity site(new gal::Entity);
    if (auto old = ss->getDock(marketId)) {
        site = old;
        is_new = false;
    }
    else if (auto old = ss->getDock(name)) {
        site = old;
        is_new = false;
    }
    std::string type;
    if (jb["type"].is_string()) {
        type = jb["type"].as_string();
        if (type == "Planetary Outpost" && name == "Stronghold Carrier")
            type = "StrongholdCarrier";
    }
    else if (jb["latitude"].is_number())
        type = "PlanetaryInstallation";

    if (auto typeNav = enum_cast<TypeNav>(type); typeNav.has_value()) {
        site->type = typeNav.value();
    } else {
        for (auto nt: gal::ALL_NAV_TYPES) {
            if (nt->match_name(name)) {
                site->type = nt->type;
                break;
            }
        }
        if (site->type == TypeNav::Other) {
            TypeNav tp = TypeNav::Other;
            for (auto nt: gal::ALL_NAV_TYPES) {
                if (nt->match_type(type)) {
                    site->type = nt->type;
                    break;
                }
            }
        }
        //if (typeNav == TypeNav::PlanetaryPort) {
        //    if (jb["government"].is_string() && jb["government"].as_string() == "$government_Engineer;")
        //        site->type = TypeNav::EngineerPort;
        //}
    }
    site->parentBodyId = parentBodyId;
    site->setName(name);
    if (auto* nt = gal::NavType::findNavType(site->type); nt && !nt->name_pattern && !nt->name_loc.empty())
        site->nloc = nt->get_nloc();

    site->marketId = marketId;
    if (is_new)
        ss->stations.push_back(site);
    if (site->marketId && !jb["market"].empty()) {
        loadMarket(site->marketId, site->name, "", ss->systemName, jb["market"]);
    }
}

static void parseBody(const gal::spStarSystem& ss, const js::value& jb) {
    gal::spEntity body(new gal::Entity);
    if (jb["type"].is_string()) {
        auto type = jb["type"].as_string();
        if (type == "Star") body->type = TypeNav::Star;
        else if (type == "Planet") body->type = TypeNav::Planet;
        else if (type == "AsteroidCluster") body->type = TypeNav::AsteroidCluster;
        else if (type == "Ring") body->type = TypeNav::Ring;
        else if (type == "Barycentre") body->type = TypeNav::Barycenter;
        else
            body->type = TypeNav::Body;
    }
    parseBodyId(ss, body, jb);

    bool is_new = true;
    if (auto b = ss->getBodyById(body->bodyId)) {
        body = b;
        is_new = false;
    }
    else if (jb["name"].is_string()) {
        auto name = jb["name"].as_string();
        if (auto b = ss->getBody(name); b && b->bodyId < 0) {
            TypeNav tp = body->type;
            body = b;
            body->type = tp;
            is_new = false;
        }
        else
            body->setName(name);
    }
    if (jb["distanceToArrival"].is_number())
        body->main_star_distance = dist_t(dist_t::LS, jb["distanceToArrival"].as_real());
    if (body->type == TypeNav::Star) {
        if (jb["solarRadius"].is_number())
            body->radius = jb["solarRadius"].as_real() * 6.957e5; // KM
        if (jb["spectralClass"].is_string())
            body->code = jb["spectralClass"].as_string();
        body->special = jb["isMainStar"].as_bool_or();
    }
    else if (body->type == TypeNav::Planet) {
        body->radius = jb["radius"].as_real_or(); // KM
        body->special = jb["isLandable"].as_bool_or();
    }

    if (is_new)
        ss->bodies.push_back(body);

    for (auto& js : jb["stations"].as_array_or()) {
        parseStation(ss, js, body->bodyId);
    }
}

gal::spStarSystem Spansh::loadStarSystem(int64_t systemAddress) {
    LOG(INFO) << "Spansh query system by id: " << systemAddress;
    TRY {
        auto cr = cpr::Get(cpr::Url{API+"dump/"+std::to_string((uint64_t)systemAddress)});
        auto cr_body = getJS(cr);
        if (cr_body["system"]["id64"].as_int_or() != systemAddress)
        return {};

        const js::value jsystem = cr_body["system"];

        auto systemName = jsystem["name"].as_string();
        cv::Point3d systemPos{jsystem["coords"]["x"].as_real_or(),
                              jsystem["coords"]["y"].as_real_or(),
                              jsystem["coords"]["z"].as_real_or()};
        gal::spStarSystem ss = gal::makeStarSystem(systemName, systemAddress, &systemPos, false);
        parseTimestampString(jsystem["date"].as_string_or(), ss->eddn_updated_at);
        int body_count = jsystem["bodyCount"].as_int_or();
        if (body_count > 0 && body_count != ss->game_body_count)
        ss->game_body_count = body_count;

        for (auto& jb : jsystem["bodies"].as_array_or()) {
            parseBody(ss, jb);
        }

        for (auto& jb : jsystem["stations"].as_array_or()) {
            parseStation(ss, jb);
        }

        ss->saved = false;
        ss->save();
        ss->loaded = true;
        return ss;
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in Spansh::loadStarSystem(int): " << e.what() << "\n" << GET_EXCEPTION_STACK_TRACE;
    }
    return {};
}

gal::spStarSystem Spansh::loadStarSystem(std::string_view name) {
    auto db_ss = db::loadStarSystem(name);
    if (db_ss.id)
        return loadStarSystem(db_ss.id);

    LOG(INFO) << "Spansh query system id by name: " << name;
    TRY {
        // https://spansh.co.uk/api/systems/field_values/system_names?q={systenName}
        std::string url = API+"systems/field_values/system_names?q=";
        char* name_esc = curl_escape(name.data(), (int)name.length());
        std::string name_plus = name_esc;
        curl_free(name_esc);
        size_t pos = 0;
        while ((pos = name_plus.find("%20", pos)) != std::string::npos) {
            name_plus.replace(pos, 3, "+");
            pos += 1; // Move past the newly inserted '+'
        }
        url += name_plus;
        auto cr = cpr::Get(cpr::Url{url});
        auto cr_body = getJS(cr);

        for (auto &ss: cr_body["min_max"].as_array_or()) {
            if (ss["name"].as_string_or() == name) {
                return loadStarSystem(ss["id64"].as_int());
            }
        }
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in Spansh::loadStarSystem(string): " << e.what() << "\n" << GET_EXCEPTION_STACK_TRACE;
    }

    return {};
}

std::vector<gal::spStarSystem> Spansh::listNearestSystems(const std::string& systemBegin, const std::string& systemEnd, double distance) {
    if (systemBegin.empty())
        return {};
    js::value j_request = js::object({
        { "filters",          js::object({{"distance", js::object({{"min", 0}, {"max", distance}})} })},
        { "size",             100},
        { "page",             0},
        { "sort",             js::object({{"distance", js::object({{"direction", "asc"}})} })},
    });
    if (systemEnd.empty() || systemBegin == systemEnd)
        j_request["reference_system"] = systemBegin;
    else
        j_request["reference_route"] = js::object({{"source", systemBegin }, {"destination", systemEnd}});

    std::vector<gal::spStarSystem> result;
    for (int page=0; page < 10; page++) {
        j_request["page"] = page;

        std::ostringstream os;
        os << std::fixed << std::setprecision(5) << js::rule::ecma404() << js::rule::no_object_nulls() << j_request;
        auto payload = os.str();

        auto cr = cpr::Post(cpr::Url{API+"systems/search"}, cpr::Body{payload});
        auto cr_body = getJS(cr);
        if (cr_body.empty())
            break;

        int position = cr_body["from"].as_int_or();
        int count = cr_body["count"].as_int_or();
        int size = cr_body["size"].as_int_or();

        auto jresult = cr_body["results"].as_array_or();
        for (auto jr : jresult) {
            auto name = jr["name"].as_string();
            int64_t address = jr["id64"].as_int();
            double x = jr["x"].as_real_or();
            double y = jr["y"].as_real_or();
            double z = jr["z"].as_real_or();
            auto total_bodies = jr["body_count"].as_int_or();
            Timestamp updated_at;
            if (jr["updated_at"].is_string())
                parseTimestampString(jr["updated_at"].as_string(), updated_at);
            cv::Point3d pos = {x,y,z};
            gal::spStarSystem ss = gal::makeStarSystem(name, address, &pos, false);
            if (updated_at < ss->eddn_updated_at || !ss->loaded)
                loadStarSystem(address);
            if (ss) {
                LOG_INFO("Star system: {} / {} (at x={:.5f} y={:.5f} z={:.5f}) has {} bodies, updated at {}",
                         name, address, x, y, z, total_bodies, updated_at);
                result.push_back(ss);
            }
        }

        if (position+size >= count)
            break;
    }

    return result;
}

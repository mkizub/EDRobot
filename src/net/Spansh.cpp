//
// Created by mkizub on 10.07.2026.
//
#include "../pch.h"

#include "Spansh.h"
#include "HttpInterceptor.h"
#include "../Galaxy.h"

#include <curl/curl.h>
#include <cpr/cpr.h>

const std::string API = "https://spansh.co.uk/api/";

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
    else if (ss->getDock(name)) {
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
        std::string type = jb["type"].as_string();
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
    if (ss->getBodyById(body->bodyId)) {
        body = ss->getBodyById(body->bodyId);
        is_new = false;
    }
    if (jb["name"].is_string()) {
        auto& name = jb["name"].as_string();
        if (ss->getBody(name)) {
            body = ss->getBodyById(body->bodyId);
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
    LOG(INFO) << "Spansh query system by id";
    auto cr = cpr::Get(cpr::Url{API+"dump/"+std::to_string((uint64_t)systemAddress)});
    auto cr_body = getJS(cr);
    if (cr_body["system"]["id64"].as_int_or() != systemAddress)
        return {};

    const js::value jsystem = cr_body["system"];

    auto systemName = jsystem["name"].as_string();
    gal::spStarSystem ss = gal::makeStarSystem(systemName, systemAddress, false);
    ss->starPos.x = jsystem["coords"]["x"].as_real_or();
    ss->starPos.y = jsystem["coords"]["y"].as_real_or();
    ss->starPos.z = jsystem["coords"]["z"].as_real_or();

    for (auto& jb : jsystem["bodies"].as_array_or()) {
        parseBody(ss, jb);
    }

    for (auto& jb : jsystem["stations"].as_array_or()) {
        parseStation(ss, jb);
    }

    ss->saved = false;
    ss->save();

    return ss;
}

gal::spStarSystem Spansh::loadStarSystem(const std::string& name) {

    // https://spansh.co.uk/api/systems/field_values/system_names?q={systenName}

    LOG(INFO) << "Spansh query system id by name";
    std::string url = API+"systems/field_values/system_names?q=";
    char* name_esc = curl_escape(name.data(), name.length());
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

    return {};
}


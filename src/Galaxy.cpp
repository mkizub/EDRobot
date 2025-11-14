//
// Created by mkizub on 22.08.2025.
//

#include "pch.h"

#include "Galaxy.h"
#include "ai/Types.h"

#include <curl_easy.h>

namespace gal {

static std::unordered_map<int64_t,spMarket > gMarketById;
static std::unordered_map<std::string,spStarSystem> gSystemsByNameCache;
static spStarSystem gCurrentStarSystem;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void saveMarket(Market* market) {
    if (!market || !market->marketId)
        return;

    json5pp::value jm = json5pp::object({
        {"timestamp", formatTimestampString(market->timestamp)},
        {"MarketID", market->marketId},
        {"StationName", market->stationName},
        {"StationType", market->stationType},
        {"Items", json5pp::array({})},
        });
    auto& jarr = jm.as_object()["Items"].as_array();
    for (auto it : market->items) {
        MarketLine& ml = it.second;
        if (!ml.isConsumer && !ml.isProducer && !ml.stock && !ml.demand)
            continue;
        json5pp::value& jv = jarr.emplace_back(json5pp::object({{"Name", it.first->nameId}}));
        auto& jo = jv.as_object();
        if (ml.isProducer)
            jo.emplace("Producer", ml.isProducer);
        if (ml.buyPrice)
            jo.emplace("BuyPrice", ml.buyPrice);
        if (ml.stock)
            jo.emplace("Stock", ml.stock);
        if (ml.isConsumer)
            jo.emplace("Consumer", ml.isConsumer);
        if (ml.sellPrice)
            jo.emplace("SellPrice", ml.sellPrice);
        if (ml.demand)
            jo.emplace("Demand", ml.demand);
    }

    std::filesystem::path fp("cache/markets/"+std::to_string(market->marketId)+".json");
    std::ofstream ofs(fp);
    ofs << json5pp::rule::ecma404() << json5pp::rule::space_indent<1>() << jm;
    ofs.close();
}

spMarket loadMarket(int64_t marketId) {
    if (!marketId)
        return {};

    json5pp::value jm;
    try {
        std::filesystem::path fp("cache/markets/"+std::to_string(marketId)+".json");
        std::ifstream ifs(fp);
        jm = json5pp::parse5(ifs);
    } catch (...) {
        return {};
    }

    spMarket market = std::make_shared<Market>();
    if (!parseTimestamp(jm["timestamp"], market->timestamp))
        return {};
    market->marketId = jm["MarketID"].as_int64();
    market->stationName = jm["StationName"].as_string();
    market->stationType = jm["StationType"].as_string();
    auto& items = jm["Items"].as_array();
    for (auto it : items) {
        Commodity* commodity = Cfg.getCommodityById(it["Name"].as_string());
        if (!commodity)
            continue;
        MarketLine ml {};
        if (it["BuyPrice"].is_integer())
            ml.buyPrice = it["BuyPrice"].as_integer();
        if (it["SellPrice"].is_integer())
            ml.sellPrice = it["SellPrice"].as_integer();
        if (it["Stock"].is_integer())
            ml.stock = it["Stock"].as_integer();
        if (it["Demand"].is_integer())
            ml.demand = it["Demand"].as_integer();
        if (it["Consumer"].is_boolean())
            ml.isConsumer = it["Consumer"].as_boolean();
        if (it["Producer"].is_boolean())
            ml.isProducer = it["Producer"].as_boolean();
        market->items.emplace(commodity, ml);
    }

    return market;
}

static json5pp::value curlRequestEDSM(std::string url, std::string systemName) {
    json5pp::value result;
    std::string readBuffer;

    CURL* curl = curl_easy_init();
    if (!curl)
        return result;
    url += curl_easy_escape(curl, systemName.c_str(), systemName.length());

    // Set URL and perform the request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset: utf-8");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    char errbuf[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        LOG(ERROR) << "Curl error: " << errbuf;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return result;

    try {
        result = json5pp::parse5(readBuffer);
    } catch (const json5pp::syntax_error& ex) {
        LOG(ERROR) << ex.what();
    }

    return result;
}

static void parseUpdated(spEntity& entity, const json5pp::value& j) {
    auto& upd = j["updateTime"];
    if (upd.is_integer()) {
        std::chrono::seconds sec_duration(upd.as_int64());
        std::chrono::sys_time<std::chrono::seconds> sys_time(sec_duration);
        entity->updated = std::chrono::utc_clock::from_sys(sys_time);
    }
    else if (upd.is_string()) {
        parseTimestampString(upd.as_string(), entity->updated);
    }
}
static void parseBodyId(StarSystem* ss, spEntity& entity, const json5pp::value& j) {
    if (j.at("bodyId").is_integer())
        entity->bodyId = j.at("bodyId").as_integer();
    if (j["parentBodyId"].is_integer())
        entity->parentBodyId = j["parentBodyId"].as_integer();
    else if (j["parents"].is_array() && !j["parents"].as_array().empty()) {
        auto& jp = j["parents"][0].as_object();
        entity->parentBodyId = jp.begin()->second.as_integer();
    }
    else if (j["body"].is_object() && j["body"]["name"].is_string()) {
        std::string body = j["body"]["name"].as_string();
        for (auto& b : ss->bodies) {
            if (b->name == body && b->bodyId >= 0) {
                entity->parentBodyId = b->bodyId;
                break;
            }
        }
    }
}
static spStarSystem fromEDDN(json5pp::value jsystem, bool saved) {
    StarSystem* ss = new StarSystem();
    ss->systemName = jsystem["name"].as_string();
    if (jsystem["address"].is_integer())
        ss->systemAddress = jsystem["address"].as_int64();
    else if (jsystem["id64"].is_integer())
        ss->systemAddress = jsystem["id64"].as_int64();
    ss->starPos.x = jsystem["coords"]["x"].as_number();
    ss->starPos.y = jsystem["coords"]["y"].as_number();
    ss->starPos.z = jsystem["coords"]["z"].as_number();

    for (auto& jb : jsystem["bodies"].as_array()) {
        spEntity body(new Entity);
        if (jb["type"].is_string()) {
            std::string type = jb["type"].as_string();
            if (auto typeNav = enum_cast<TypeNav>(type); typeNav.has_value())
                body->type = typeNav.value();
        }
        parseUpdated(body, jb);
        parseBodyId(ss, body, jb);

        body->setName(jb["name"].as_string());
        if (jb["distanceToArrival"].is_number())
            body->main_star_distance = dist_t(dist_t::LS, jb["distanceToArrival"].as_number());
        if (body->type == TypeNav::Star) {
            body->radius = jb["solarRadius"].as_number() * 6.957e5; // KM
            if (jb["spectralClass"].is_string())
                body->code = jb["spectralClass"].as_string();
            if (jb["isMainStar"].is_boolean())
                body->special = jb["isMainStar"].as_boolean();
        }
        else if (body->type == TypeNav::Planet) {
            body->radius = jb["radius"].as_number(); // KM
            if (jb["isLandable"].is_boolean())
                body->special = jb["isLandable"].as_boolean();
        }
        ss->bodies.push_back(body);
    }

    //std::string lng = toLower(std::string(enum_name<Lang>(Cfg.lng)));
    for (auto& jb : jsystem["stations"].as_array()) {
        std::string name = jb["name"].as_string();
        spEntity site(new Entity);
        std::string type;
        if (jb["type"].is_string())
             type = jb["type"].as_string();
        if (auto typeNav = enum_cast<TypeNav>(type); typeNav.has_value()) {
            site->type = typeNav.value();
        } else {
            for (auto nt: ALL_NAV_TYPES) {
                if (nt->match_name(name)) {
                    site->type = nt->type;
                    break;
                }
            }
            if (site->type == TypeNav::Other) {
                TypeNav tp = TypeNav::Other;
                for (auto nt: ALL_NAV_TYPES) {
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
        parseUpdated(site, jb);
        parseBodyId(ss, site, jb);
        site->setName(name);

        if (jb["marketId"].is_integer())
            site->marketId = jb.at("marketId",0).as_int64();
        else if (jb["id64"].is_integer())
            site->marketId = jb.at("id64",0).as_int64();
        ss->stations.push_back(site);
    }

    ss->saved = saved;
    return spStarSystem(ss);
}

void saveStarSystem(StarSystem* ss) {
    if (!ss)
        return;
    json5pp::value jbodies = json5pp::array({});
    json5pp::value jstations = json5pp::array({});
    for (auto& body : ss->bodies) {
        json5pp::value jb {
                {"type", std::string(enum_name<TypeNav>(body->type))},
                {"name", body->name},
        };
        auto& jbo = jb.as_object();
        if (body->bodyId >= 0)
            jbo.emplace("bodyId", body->bodyId);
        if (body->parentBodyId >= 0)
            jbo.emplace("parentBodyId", body->parentBodyId);
        if (body->main_star_distance.valid())
            jbo.emplace("distanceToArrival", body->main_star_distance.get(dist_t::LS));
        if (body->type == TypeNav::Star) {
            if (body->radius)
                jbo.emplace("solarRadius", body->radius / 6.957e5);
            if (body->special)
                jbo.emplace("isMainStar", true);
            if (!body->code.empty())
                jbo.emplace("spectralClass", body->code);
        }
        else if (body->type == TypeNav::Planet) {
            if (body->radius)
                jbo.emplace("radius", body->radius);
            if (body->special)
                jbo.emplace("isLandable", true);
            if (!body->code.empty())
                jbo.emplace("spectralClass", body->code);
        }

        {
            auto ts = std::chrono::floor<std::chrono::seconds>(body->updated);
            auto seconds = ts.time_since_epoch().count();
            if (seconds)
                jbo.emplace("updateTime", seconds);
        }
        jbodies.as_array().push_back(jb);
    }
    for (auto& st : ss->stations) {
        json5pp::value jst {
                {"type", std::string(enum_name<TypeNav>(st->type))},
                {"name", st->name},
        };
        auto& jsto = jst.as_object();
        if (st->bodyId >= 0)
            jsto.emplace("bodyId", st->bodyId);
        if (st->parentBodyId >= 0)
            jsto.emplace("parentBodyId", st->parentBodyId);
        if (st->main_star_distance.valid())
            jsto.emplace("distanceToArrival", st->main_star_distance.get(dist_t::LS));
        if (st->marketId)
            jsto.emplace("marketId", st->marketId);
        {
            auto ts = std::chrono::floor<std::chrono::seconds>(st->updated);
            auto seconds = ts.time_since_epoch().count();
            if (seconds)
                jsto.emplace("updateTime", seconds);
        }
        jstations.as_array().push_back(jst);
    }

    json5pp::value jout {
            {"name", ss->systemName},
            {"address", ss->systemAddress},
            {"coords", json5pp::object({
                {"x", ss->starPos.x},
                {"y", ss->starPos.y},
                {"z", ss->starPos.z},
                })},
            {"bodies", jbodies},
            {"stations", jstations},
    };

    std::filesystem::path fp("cache/systems/"+ss->systemName+".json");
    std::ofstream ofs(fp);
    ofs << json5pp::rule::ecma404() << json5pp::rule::space_indent<1>() << jout;
    ofs.close();

    ss->saved = true;
}

static spStarSystem loadStarSystemFromNetwork(const std::string name) {

    std::string url = "https://www.edsm.net/api-v1/system?showId=1&showCoordinates=1&systemName=";
    json5pp::value jsystem = curlRequestEDSM(url, name);
    if (!jsystem || !jsystem["name"] || name != jsystem["name"].as_string())
        return {};

    url = "https://www.edsm.net/api-system-v1/bodies?systemName=";
    json5pp::value bodies = curlRequestEDSM(url, name);
    if (!bodies || !bodies["name"] || name != bodies["name"].as_string())
        return {};
    jsystem.as_object()["bodies"] = bodies["bodies"];

    url = "https://www.edsm.net/api-system-v1/stations?systemName=";
    json5pp::value stations = curlRequestEDSM(url, name);
    if (!stations || !stations["name"] || name != stations["name"].as_string())
        return {};
    jsystem.as_object()["stations"] = stations["stations"];

    return fromEDDN(jsystem, false);
}

static spStarSystem loadStarSystem(const std::string& name) {
    assert(!gSystemsByNameCache.contains(name));

    std::filesystem::path fp("cache/systems/"+name+".json");
    if (!std::filesystem::exists(fp))
        return loadStarSystemFromNetwork(name);

    try {
        std::ifstream ifs(fp);
        auto jsystem = json5pp::parse5(ifs);
        return fromEDDN(jsystem, true);
    } catch (...) {
        return loadStarSystemFromNetwork(name);
    }
}

spStarSystem getStarSystem(const std::string& name) {
    if (gCurrentStarSystem && gCurrentStarSystem->systemName == name)
        return gCurrentStarSystem;
    const auto& it = gSystemsByNameCache.find(name);
    if (it != gSystemsByNameCache.end() && it->second)
        return it->second;
    spStarSystem ss = loadStarSystem(name);
    if (ss && !ss->saved) {
        assert (ss->systemName == name);
        gSystemsByNameCache[ss->systemName] = ss;
        saveStarSystem(ss.get());
    }
    return ss;
}

spStarSystem getStarSystem(const std::string& name, int64_t address) {
    spStarSystem ss = getStarSystem(name);
    if (!ss) {
        ss.reset(new StarSystem());
        ss->systemName = name;
        ss->systemAddress = address;
    }
    return ss;
}

spStarSystem& getCurrentStarSystem() {
    return gCurrentStarSystem;
}
void setCurrentStarSystem(spStarSystem ss) {
    gCurrentStarSystem.swap(ss);
}

spEntity StarSystem::getMainStar() {
    for (auto& b : this->bodies) {
        if (b->type == TypeNav::Star && b->special)
            return b;
    }
    return {};
}

spEntity StarSystem::getEntity(const std::string& nm) {
    if (nm.empty())
        return {};
    for (auto& e : this->bodies) {
        if (e->nameEq(nm))
            return e;
    }
    for (auto& e : this->stations) {
        if (e->nameEq(nm))
            return e;
    }
    for (auto& e : this->signals) {
        if (e->nameEq(nm))
            return e;
    }
    return {};
}

spEntity StarSystem::getBodyById(int bodyId) {
    if (bodyId <= 0)
        return {};
    for (auto& b : this->bodies) {
        if (b->bodyId >= 0 && b->bodyId == bodyId)
            return b;
    }
    // stations also have bodyId
    for (auto& s : this->stations) {
        if (s->bodyId >= 0 && s->bodyId == bodyId)
            return s;
    }
    return {};
}

spEntity StarSystem::getBody(const std::string& bname) {
    if (bname.empty())
        return {};
    for (auto& b : this->bodies) {
        if (b->nameEq(bname))
            return b;
    }
    return {};
}
spEntity StarSystem::getDock(const std::string& sname) {
    if (sname.empty())
        return {};
    for (auto& s : this->stations) {
        if (s->nameEq(sname))
            return s;
    }
    return {};
}

spEntity StarSystem::getDock(int64_t marketId) {
    if (marketId == 0)
        return {};
    for (auto& s : this->stations) {
        if (s->marketId == marketId)
            return s;
    }
    return {};
}


void StarSystem::addDestination() {
    if (this->systemAddress != st::destination.systemAddress)
        return;
    auto& dname = st::destination.name;
    for (auto& b : this->bodies) {
        if (b->nameEq(dname))
            return;
    }
    for (auto& s : this->stations) {
        if (s->nameEq(dname)) {
            switch (s->type) {
            case TypeNav::NavBeacon:
            case TypeNav::TouristBeacon:
            case TypeNav::SpaceInstallation:
            case TypeNav::FleetCarrier:
            case TypeNav::SquadronCarrier:
            case TypeNav::StrongholdCarrier:
            case TypeNav::ColonisationShip:
            case TypeNav::Megaship:
            case TypeNav::TrailblazerDream:
            case TypeNav::PlanetaryThing:
            case TypeNav::PlanetaryStation:
            case TypeNav::PlanetaryPort:
            case TypeNav::PlanetaryInstallation:
            case TypeNav::EngineerPort:
            case TypeNav::Settlement:
            case TypeNav::SpaceConstrDepot:
            case TypeNav::PlanetaryConstrDepot:
                if (s->parentBodyId != st::destination.bodyId) {
                    s->parentBodyId = st::destination.bodyId;
                    this->saved = false;
                    saveStarSystem(this);
                }
            }
            return;
        }
    }
    if (dname.size() > this->systemName.size() && dname.starts_with(this->systemName)) {
        if (dname.size() == this->systemName.size()+2) {
            if (dname[dname.size()-2] != ' ')
                return;
            // a star
            char c = dname[dname.size()-1];
            if (c >= 'A' && c <= 'Z') {
                spEntity star = std::make_shared<Entity>();
                star->setName(dname);
                star->bodyId = st::destination.bodyId;
                bodies.push_back(star);
                this->saved = false;
            }
        } else {
            // a body
            spEntity body = std::make_shared<Entity>();
            body->setName(dname);
            body->bodyId = st::destination.bodyId;
            bodies.push_back(body);
            this->saved = false;
        }
        return;
    }
    else if (dname.starts_with("Orbital Construction Site:")) {
        spEntity site = std::make_shared<Entity>();
        site->type = TypeNav::SpaceConstrDepot;
        site->setName(dname);
        site->parentBodyId = st::destination.bodyId;
        stations.push_back(site);
        this->saved = false;
    }
    else if (dname.starts_with("Planetary Construction Site:")) {
        spEntity site = std::make_shared<Entity>();
        site->type = TypeNav::PlanetaryConstrDepot;
        site->setName(dname);
        site->parentBodyId = st::destination.bodyId;
        stations.push_back(site);
        this->saved = false;
    }
    if (!this->saved)
        saveStarSystem(this);
}

spEntity StarSystem::addNavListEntry(wchar_t charOCR, const std::string& nav_icon, const std::string& sname, int bodyId) {
    spEntity entity = getEntity(sname);
    bool added = false;
    if (!entity) {
        entity.reset(new Entity);
        added = true;
    }

    TypeNav typeNav = TypeNav::Other;
    for (auto nt: ALL_NAV_TYPES) {
        if (nt->match_name(sname)) {
            typeNav = nt->type;
            break;
        }
    }
    if (typeNav == TypeNav::Other) {
        TypeNav tp = TypeNav::Other;
        for (auto nt: ALL_NAV_TYPES) {
            if (nt->match_icon(charOCR, nav_icon)) {
                if (entity->type == TypeNav::Other || nt->type == entity->type) {
                    typeNav = nt->type;
                    break;
                }
                if (tp != TypeNav::Other)
                    tp = nt->type;
            }
        }
        if (typeNav != entity->type && tp != TypeNav::Other)
            typeNav = tp;
    }

    if (typeNav != TypeNav::Other && entity->type != typeNav) {
        entity->type = typeNav;
        saved = false;
    }
    if (entity->name.empty() || !entity->nameEq(sname)) {
        entity->setName(sname);
        saved = false;
    }

    if (bodyId >= 0) {
        if (isBody(entity->type) || isSpaceStation(entity->type)) {
            if (entity->bodyId < 0) {
                entity->bodyId = bodyId;
                saved = false;
            }
        } else {
            if (entity->parentBodyId < 0) {
                entity->parentBodyId = bodyId;
                saved = false;
            }
        }
    }

    if (added) {
        if (isBody(entity->type)) {
            bodies.push_back(entity);
            saved = false;
        }
        else if (isSite(entity->type)) {
            stations.push_back(entity);
            saved = false;
        }
        else
            signals.push_back(entity);
    }
    if (!saved)
        saveStarSystem(this);
    return entity;
}

spEntity StarSystem::addStation(spGameEvent& ge) {
    if (!(ge->event=="ApproachSettlement" || ge->event=="Docked" || (ge->event=="Location") && ge->data["Docked"]))
        return {};
    auto& je = ge->data;

    std::string sname = je.at("StationName","").as_string();
    std::string stype = je.at("StationType","").as_string();
    int64_t marketId = je.at("MarketID",0).as_int64();
    spEntity dock = getDock(marketId);
    if (!dock) {
        dock = getDock(sname);
        if (dock) {
            if (dock->marketId) {
                assert (dock->marketId != marketId);
                dock.reset();
            } else {
                dock->marketId = marketId;
                saved = false;
            }
        }
    }
    if (!dock) {
        dock.reset(new Entity);
        dock->marketId = marketId;
    }

    TypeNav typeNav = TypeNav::Other;
    for (auto nt: ALL_NAV_TYPES) {
        if (nt->match_name(sname)) {
            typeNav = nt->type;
            break;
        }
    }
    if (typeNav == TypeNav::Other) {
        TypeNav tp = TypeNav::Other;
        for (auto nt: ALL_NAV_TYPES) {
            if (nt->match_type(stype)) {
                if (nt->type == dock->type) {
                    typeNav = nt->type;
                    break;
                }
                if (tp != TypeNav::Other)
                    tp = nt->type;
            }
        }
        if (typeNav != dock->type && tp != TypeNav::Other)
            typeNav = tp;
    }
    if (typeNav == TypeNav::PlanetaryPort) {
        if (je["StationGovernment"].is_string() && je["StationGovernment"].as_string() == "$government_Engineer;")
            typeNav = TypeNav::EngineerPort;
    }
    if (typeNav != TypeNav::Other && dock->type != typeNav) {
        dock->type = typeNav;
        saved = false;
    }
    if (dock->name.empty() || dock->nameEq(sname)) {
        dock->setName(sname);
        saved = false;
    }
    if (dock->parentBodyId < 0) {
        if (st::space.bodyType == "Planet" || st::space.bodyType == "Star") {
            dock->parentBodyId = st::space.bodyId;
            saved = false;
        }
    }
    if (je["DistFromStarLS"].is_number()) {
        double dist = je["DistFromStarLS"].as_number();
        if (!dock->main_star_distance.valid() || std::round(dock->main_star_distance.get(dist_t::LS)) != std::round(dist)) {
            dock->main_star_distance = dist_t(dist_t::LS, dist);
            saved = false;
        }
    }
    if (!saved)
        saveStarSystem(this);
    return dock;
}


spEntity StarSystem::addSignal(spEntity signal) {
    if (signal) {
        assert (isSignal(signal->type));
        signals.push_back(signal);
    }
    return signal;
}

void StarSystem::checkType(spEntity& site, TypeNav type, Timestamp timestamp) {
    if (site && type != TypeNav::Other && site->type != type && site->updated < timestamp) {
        site->type = type;
        site->updated = timestamp;
        saved = false;
    }
}
void StarSystem::checkName(spEntity& site, const std::string& name, Timestamp timestamp) {
    if (site && !site->nameEq(name) && site->updated < timestamp) {
        if (site->setName(name)) {
            site->updated = timestamp;
            saved = false;
        }
    }
}
void StarSystem::checkNloc(spEntity& site, const std::string& nloc, Timestamp timestamp) {
    if (site && site->nloc != nloc) {
        site->nloc = nloc;
        site->updated = timestamp;
    }
}
void StarSystem::addFSSSignalDiscovered(std::vector<std::shared_ptr<GameEvent>>& events) {
    if (events.empty())
        return;
    Timestamp timestamp = events.front()->timestamp;
    // add all sites from events
    for (auto event : events) {
        if (event->event != "FSSSignalDiscovered")
            return;
        json5pp::value &data = event->data;
        if (auto &ja = data["SystemAddress"]; !ja.is_integer() || this->systemAddress != ja.as_int64())
            return;
        std::string stype = data["SignalType"].as_string();
        std::string sname = data["SignalName"].as_string();
        std::string snloc;
        if (data["SignalName_Localised"].is_string()) {
            snloc = data["SignalName_Localised"].as_string();
            sname = snloc;
        }
        bool isStation = data["IsStation"];

        spEntity site = getDock(sname);
        if (!site) {
            site.reset(new Entity());
            if (stype == "NavBeacon")
                site->type = TypeNav::NavBeacon;
            if (stype == "FleetCarrier")
                site->type = TypeNav::FleetCarrier;
            site->setName(sname);
            stations.push_back(site);
            saved = false;
        }
        else if (site->updated > timestamp)
            continue;

        TypeNav typeNav = TypeNav::Other;
        for (auto nt: ALL_NAV_TYPES) {
            if (nt->match_name(sname)) {
                typeNav = nt->type;
                break;
            }
        }
        if (typeNav == TypeNav::Other) {
            TypeNav tp = TypeNav::Other;
            for (auto nt: ALL_NAV_TYPES) {
                if (nt->match_type(stype)) {
                    if (nt->type == site->type) {
                        typeNav = nt->type;
                        break;
                    }
                    if (tp != TypeNav::Other)
                        tp = nt->type;
                }
            }
            if (typeNav != site->type && tp != TypeNav::Other)
                typeNav = tp;
        }
        checkType(site, typeNav, timestamp);
        checkName(site, sname, timestamp);
        checkNloc(site, snloc, timestamp);
    }
    if (!saved)
        saveStarSystem(this);
}

spMarket getMarket(int64_t marketId) {
    if (!marketId)
        return {};
    auto market = gMarketById[marketId];
    if (market)
        return market;
    market = loadMarket(marketId);
    if (market)
        gMarketById[marketId] = market;
    return market;
}

void setMarketData(spMarket market) {
    if (!market || !market->marketId)
        return;
    if (auto old = gMarketById[market->marketId]; old && old->timestamp > market->timestamp)
        return;
    gMarketById[market->marketId] = market;
    saveMarket(market.get());
}

bool Entity::nameEq(const std::string& nm) const {
    if (name == nm)
        return true;
    if (!nloc.empty() && nloc == nm)
        return true;

    NavType* navType = nullptr;
    switch (type) {
    case TypeNav::Other:
    case TypeNav::Error:
    case TypeNav::Signal:
    case TypeNav::StarSystem:
    case TypeNav::Body:
    case TypeNav::Barycenter:
    case TypeNav::Ring:
    case TypeNav::AsteroidCluster:
    case TypeNav::Star:
    case TypeNav::Planet:
    case TypeNav::SpaceThing:
    case TypeNav::TouristBeacon:
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceInstallation:
    case TypeNav::SquadronCarrier:
    case TypeNav::ColonisationShip:
    case TypeNav::PlanetaryThing:
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryInstallation:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::PlanetaryConstrDepot:
    case TypeNav::Megaship:
    case TypeNav::StationMegaShip:
        return false;
    case TypeNav::FleetCarrier:
        if (code == nm)
            return true;
        if (name.size() == 7 && nm.size() > 7 && nm.ends_with(name))
            return true;
        if (nm.size() == 7 && name.size() > 7 && name.ends_with(nm))
            return true;
        return false;
    case TypeNav::NotExplored:
        navType = &UNEXPLORED;
        break;
    case TypeNav::WarZone:
        navType = &WAR_ZONE;
        break;
    case TypeNav::ResSite:
        navType = &RES_SITE;
        break;
    case TypeNav::NavBeacon:
        navType = &BEACON;
        break;
    case TypeNav::StrongholdCarrier:
        navType = &STRONGHOLD_CARRIER;
        break;
    case TypeNav::TrailblazerDream:
        navType = &TRAILBLAZER_DREAM;
    }
    for (auto& p : navType->name_loc) {
        if (p.second == nm)
            return true;
    }
    return false;
}

bool Entity::setName(const std::string& nm) {
    NavType* navType = nullptr;
    switch (type) {
    case TypeNav::Other:
    case TypeNav::Error:
    case TypeNav::Signal:
    case TypeNav::StarSystem:
    case TypeNav::Body:
    case TypeNav::Barycenter:
    case TypeNav::Ring:
    case TypeNav::AsteroidCluster:
    case TypeNav::Star:
    case TypeNav::Planet:
    case TypeNav::SpaceThing:
    case TypeNav::TouristBeacon:
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceInstallation:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::Megaship:
    case TypeNav::StationMegaShip:
    case TypeNav::SquadronCarrier:
    case TypeNav::PlanetaryThing:
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryInstallation:
    case TypeNav::PlanetaryConstrDepot:
        if (name == nm)
            return false;
        name = nm;
        return true;
    case TypeNav::FleetCarrier:
        if (name == nm)
            return false;
        if (nm.size() == 7)
            code = nm;
        else if (nm.size() > 7)
            code = nm.substr(nm.size()-7);
        name = nm;
        return true;
    case TypeNav::NotExplored:
        navType = &UNEXPLORED;
        break;
    case TypeNav::WarZone:
        navType = &WAR_ZONE;
        break;
    case TypeNav::ResSite:
        navType = &RES_SITE;
        break;
    case TypeNav::NavBeacon:
        navType = &BEACON;
        break;
    case TypeNav::StrongholdCarrier:
        navType = &STRONGHOLD_CARRIER;
        break;
    case TypeNav::TrailblazerDream:
        navType = &TRAILBLAZER_DREAM;
        break;
    case TypeNav::ColonisationShip:
        navType = &COLONIZATION_SHIP;
        break;
    }

    std::string xx_name;
    std::string ru_name;
    auto it = navType->name_loc.begin();
    for (; it != navType->name_loc.end(); it++) {
        if (it->first == Lang::XX || it->first == Lang::EN)
            xx_name = it->second;
        if (it->second == nm)
            break;
    }
    if (it == navType->name_loc.end()) {
        xx_name.clear();
    } else {
        for (; it != navType->name_loc.end(); it++) {
            if (it->first == Lang::XX || it->first == st::lng) {
                ru_name = it->second;
                break;
            }
        }
    }
    if (xx_name.empty()) {
        xx_name.clear();
        ru_name.clear();
        it = navType->name_loc.begin();
        for (; it != navType->name_loc.end(); it++) {
            if (xx_name.empty() && (it->first == Lang::XX || it->first == Lang::EN)) {
                xx_name = it->second;
                break;
            }
        }
        for (; it != navType->name_loc.end(); it++) {
            if (it->first == Lang::XX || it->first == st::lng) {
                ru_name = it->second;
                break;
            }
        }
    }
    if (this->name != xx_name) {
        this->name = xx_name;
        if (ru_name.empty() || ru_name == xx_name)
            this->nloc.clear();
        else
            this->nloc = ru_name;
        return true;
    }
    else if (this->nloc != ru_name) {
        this->nloc = ru_name;
    }
    return false;
}


} // namespace gal

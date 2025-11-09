//
// Created by mkizub on 22.08.2025.
//

#include "pch.h"

#include "Galaxy.h"

#include <curl_easy.h>

namespace gal {

static std::unordered_map<std::string,spStarSystem> gSystemsByNameCache;
static spStarSystem gCurrentStarSystem;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

json5pp::value exportMarketData(Market* market) {
    if (!market || market->items.empty())
        return {};

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
    return jm;
}

spMarket importMarketData(const json5pp::value& jm) {
    if (jm.is_null())
        return {};

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
        spBody body;
        TypeNav typeNav = TypeNav::Other;
        if (jb["typeNav"]) {
            typeNav = enum_cast<TypeNav>(jb["typeNav"].as_string()).value();
        } else {
            std::string type;
            if (jb["type"].is_string())
                type = jb["type"].as_string();
            if (type == "Star")
                typeNav = TypeNav::Star;
            else if (type == "Planet")
                typeNav = TypeNav::Planet;
        }
        if (typeNav == TypeNav::Star)
            body.reset(new Star());
        else if (typeNav == TypeNav::Planet)
            body.reset(new Planet());
        else
            body.reset(new Body());
        body->typeNav = typeNav;

        body->name = jb["name"].as_string();
        if (jb.at("bodyId").is_integer())
            body->bodyId = jb.at("bodyId").as_integer();
        body->parentBodyId = -1;
        if (jb["distance"].is_number())
            body->distance = dist_t(dist_t::LS, jb["distance"].as_number());
        else if (jb["distanceToArrival"].is_number())
            body->distance = dist_t(dist_t::LS, jb["distanceToArrival"].as_number());
        body->radius = 0;
        if (body->typeNav == TypeNav::Star) {
            Star* star = (Star*) body.get();
            star->radius = jb["solarRadius"].as_number() * 6.957e5; // KM
            if (jb["spectralClass"].is_string())
                star->spectralClass = jb["spectralClass"].as_string();
            if (jb["isMainStar"].is_boolean())
                star->isMainStar = jb["isMainStar"].as_boolean();
            if (jb["isScoopable"].is_boolean())
                star->isScoopable = jb["isScoopable"].as_boolean();
        }
        else if (body->typeNav == TypeNav::Planet) {
            Planet* planet = (Planet*) body.get();
            planet->radius = jb["radius"].as_number(); // KM
            if (jb["isLandable"].is_boolean())
                planet->isLandable = jb["isLandable"].as_boolean();
        }
        if (jb["parentBodyId"].is_integer())
            body->parentBodyId = jb["parentBodyId"].as_integer();
        else if (jb["parents"].is_array() && !jb["parents"].as_array().empty()) {
            auto& jp = jb["parents"][0].as_object();
            body->parentBodyId = jp.begin()->second.as_integer();
        }
        ss->bodies.push_back(body);
    }

    //std::string lng = toLower(std::string(enum_name<Lang>(Cfg.lng)));
    for (auto& jb : jsystem["stations"].as_array()) {
        std::string name = jb["name"].as_string();
        TypeNav typeNav = TypeNav::Other;
        TypeSite typeSite = TypeSite::Other;
        if (jb["typeNav"]) {
            typeNav = enum_cast<TypeNav>(jb["typeNav"].as_string()).value();
            if (jb["typeSite"])
                typeSite = enum_cast<TypeSite>(jb["typeSite"].as_string()).value();
        } else {
            std::string type;
            if (jb["type"].is_string())
                type = jb["type"].as_string();
            if (type == "Drake-Class Carrier" || type == "Fleet Carrier") {
                typeNav = TypeNav::Carrier;
                typeSite = TypeSite::FleetCarrier;
            } else if (type == "Squadron Carrier") {
                typeNav = TypeNav::Carrier;
                typeSite = TypeSite::SquadronCarrier;
            } else if (type == "Orbis" || type == "Orbis Starport") {
                typeNav = TypeNav::SpacePort;
                typeSite = TypeSite::Orbis;
            } else if (type == "Ocellus" || type == "Ocellus Starport") {
                typeNav = TypeNav::SpacePort;
                typeSite = TypeSite::Ocellus;
            } else if (type == "Coriolis" || type == "Coriolis Starport") {
                typeNav = TypeNav::SpacePort;
                typeSite = TypeSite::Coriolis;
            } else if (type == "Outpost" || type == "Outpost Starport") {
                typeNav = TypeNav::SpacePort;
                typeSite = TypeSite::SpaceOutpost;
            } else if (type == "SpaceConstr" || type == "SpaceConstructionDepot") {
                typeNav = TypeNav::SpaceConstr;
                typeSite = TypeSite::SpaceConstr;
            } else if (type == "PlanetConstr" || type == "PlanetaryConstructionDepot") {
                typeNav = TypeNav::PlanetConstr;
                typeSite = TypeSite::PlanetConstr;
            }
            else if (/*type == "Planetary Outpost" &&*/ name == "Stronghold Carrier" || name == "Носитель-база") {
                typeNav = TypeNav::MegashipDock;
                typeSite = TypeSite::StrongholdCarrier;
            } else if (/*type == "Planetary Outpost" &&*/ name == "Trailblazer Dream") {
                typeNav = TypeNav::MegashipDock;
                typeSite = TypeSite::TrailblazerDream;
            } else if (/*type == "Planetary Outpost" &&*/ name.starts_with("$EXT_PANEL_ColonisationShip;")) {
                typeNav = TypeNav::MegashipDock;
                typeSite = TypeSite::ColonisationShip;
            } else if (type == "Planetary Outpost") {
                typeNav = TypeNav::PlanetInst;
                typeSite = TypeSite::Other;
            } else if (type == "Settlement" || type == "Odyssey Settlement") {
                typeNav = TypeNav::PlanetPort;
                typeSite = TypeSite::Settlement;
            }
        }
        spSite site;
        site.reset(new Site());
        site->typeNav = typeNav;
        site->typeSite = typeSite;
        if (auto& upd = jb.at("updated")) {
            if (upd.is_integer()) {
                std::chrono::seconds sec_duration(jb.at("updated").as_int64());
                std::chrono::sys_time<std::chrono::seconds> sys_time(sec_duration);
                site->updated = std::chrono::utc_clock::from_sys(sys_time);
            }
            else if (upd.is_string()) {
                parseTimestampString(upd.as_string(), site->updated);
            }
        }
        if (jb.at("bodyId").is_integer())
            site->bodyId = jb.at("bodyId").as_integer();
        site->name = name;
        if (typeSite == TypeSite::StrongholdCarrier && st::lng == Lang::RU)
            site->nloc = "Носитель-база";
        if (typeSite == TypeSite::NavBeacon && st::lng == Lang::RU)
            site->nloc = "Нав. маяк";
        site->parentBodyId = -1;
        if (jb["marketId"].is_integer())
            site->marketId = jb.at("marketId",0).as_int64();
        else if (jb["id64"].is_integer())
            site->marketId = jb.at("id64",0).as_int64();
        if (jb["marketData"].is_object())
            site->marketData = importMarketData(jb["marketData"]);
        //if (jb["nloc"] && jb["nloc"][lng])
        //    st->nloc = jb["nloc"][lng];
        if (jb["parentBodyId"].is_integer())
            site->parentBodyId = jb["parentBodyId"].as_integer();
        else if (jb["parents"].is_array() && !jb["parents"].as_array().empty()) {
            auto& jp = jb["parents"][0].as_object();
            site->parentBodyId = jp.begin()->second.as_integer();
        }
        else if (jb["body"].is_object() && jb["body"]["name"].is_string()) {
            std::string body = jb["body"]["name"].as_string();
            for (auto& b : ss->bodies) {
                if (b->name == body && b->bodyId.has_value()) {
                    site->parentBodyId = b->bodyId.value();
                    break;
                }
            }
        }
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
                {"name", body->name},
                {"distance", body->distance.dist},
        };
        auto& jbo = jb.as_object();
        if (body->parentBodyId >= 0)
            jbo.emplace("parentBodyId", body->parentBodyId);
        if (body->bodyId.has_value())
            jbo.emplace("bodyId", body->bodyId.value());
        jbo.emplace("typeNav", std::string(enum_name<TypeNav>(body->typeNav)));
        if (body->typeNav == TypeNav::Star) {
            Star* s = (Star*)body.get();
            if (s->radius)
                jbo.emplace("solarRadius", s->radius / 6.957e5);
            if (s->isMainStar)
                jbo.emplace("isMainStar", s->isMainStar);
            if (s->isScoopable)
                jbo.emplace("isScoopable", s->isScoopable);
            if (!s->spectralClass.empty())
                jbo.emplace("spectralClass", s->spectralClass);
        }
        else if (body->typeNav == TypeNav::Planet) {
            Planet* p = (Planet*)body.get();
            if (p->radius)
                jbo.emplace("radius", p->radius);
            if (p->isLandable)
                jbo.emplace("isLandable", p->isLandable);
        }
        jbodies.as_array().push_back(jb);
    }
    for (auto& st : ss->stations) {
        json5pp::value jst {
                {"name", st->name},
        };
        auto& jsto = jst.as_object();
        jsto.emplace("typeNav", std::string(enum_name<TypeNav>(st->typeNav)));
        if (st->typeSite != TypeSite::Other)
            jsto.emplace("typeSite", std::string(enum_name<TypeSite>(st->typeSite)));
        if (st->bodyId.has_value())
            jsto.emplace("bodyId", st->bodyId.value());
        if (st->parentBodyId >= 0)
            jsto.emplace("parentBodyId", st->parentBodyId);
        if (st->marketId)
            jsto.emplace("marketId", st->marketId);
        if (st->marketData && !st->marketData->items.empty())
            jsto.emplace("marketData", exportMarketData(st->marketData.get()));
        //if (!st->nloc.empty())
        //    jsto.emplace("nloc", st->nloc);
        {
            auto ts = std::chrono::floor<std::chrono::seconds>(st->updated);
            auto seconds = ts.time_since_epoch().count();
            if (seconds)
                jsto.emplace("updated", seconds);
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

Star* StarSystem::getMainStar() {
    for (auto& b : this->bodies) {
        if (b->typeNav == TypeNav::Star) {
            Star* star = (Star*)b.get();
            if (star->isMainStar)
                return star;
        }
    }
    return nullptr;
}

spItem StarSystem::getBodyById(int bodyId) {
    if (bodyId <= 0)
        return {};
    for (auto& b : this->bodies) {
        if (b->bodyId.has_value() && b->bodyId.value() == bodyId) {
            return std::static_pointer_cast<Item>(b);
        }
    }
    // stations also have bodyId
    for (auto& s : this->stations) {
        if (s->bodyId.has_value() && s->bodyId.value() == bodyId) {
            return std::static_pointer_cast<Item>(s);
        }
    }
    return {};
}

spBody StarSystem::getBody(const std::string& bname) {
    if (bname.empty())
        return {};
    for (auto& b : this->bodies) {
        if (b->name == bname) {
            return b;
        }
    }
    return {};
}
spSite StarSystem::getDock(const std::string& sname) {
    if (sname.empty())
        return {};
    for (auto& s : this->stations) {
        if (s->name == sname || s->nloc == sname) {
            return s;
        }
    }
    return {};
}

spSite StarSystem::getDock(int64_t marketId) {
    if (marketId == 0)
        return {};
    for (auto& s : this->stations) {
        if (s->marketId == marketId) {
            return s;
        }
    }
    return {};
}


void StarSystem::addDestination() {
    if (this->systemAddress != st::destination.systemAddress)
        return;
    auto& dname = st::destination.name;
    for (auto& b : this->bodies) {
        if (b->name == dname)
            return;
    }
    for (auto& s : this->stations) {
        if (s->name == dname || s->nloc == dname) {
            switch (s->typeSite) {
            case TypeSite::FleetCarrier:
            case TypeSite::SquadronCarrier:
            case TypeSite::StrongholdCarrier:
            case TypeSite::ColonisationShip:
            case TypeSite::StationMegaShip:
            case TypeSite::TrailblazerDream:
            case TypeSite::EngineerPort:
            case TypeSite::Settlement:
            case TypeSite::SpaceConstr:
            case TypeSite::PlanetConstr:
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
                spBody star = std::make_shared<Star>();
                star->name = dname;
                star->bodyId = st::destination.bodyId;
                bodies.push_back(star);
                this->saved = false;
            }
        } else {
            // a body
            spBody body = std::make_shared<Body>();
            body->name = dname;
            body->bodyId = st::destination.bodyId;
            bodies.push_back(body);
            this->saved = false;
        }
        return;
    }
    else if (dname.starts_with("Orbital Construction Site:")) {
        spSite site = std::make_shared<Site>();
        site->typeNav = gal::TypeNav::SpaceConstr;
        site->typeSite = gal::TypeSite::SpaceConstr;
        site->name = dname;
        site->parentBodyId = st::destination.bodyId;
        stations.push_back(site);
        this->saved = false;
    }
    else if (dname.starts_with("Planetary Construction Site:")) {
        spSite site = std::make_shared<Site>();
        site->typeNav = gal::TypeNav::PlanetConstr;
        site->typeSite = gal::TypeSite::PlanetConstr;
        site->name = dname;
        site->parentBodyId = st::destination.bodyId;
        stations.push_back(site);
        this->saved = false;
    }
    if (!this->saved)
        saveStarSystem(this);
}

spSite StarSystem::addStation(int64_t marketId, const std::string& sname, const std::string& type) {
    spSite site;
    if (marketId) {
        for (auto &s: stations) {
            if (s->marketId == marketId) {
                site = s;
                break;
            }
        }
    }
    if (!site) {
        for (auto &s: stations) {
            if (s->nameEq(sname)) {
                if (!marketId || !s->marketId) {
                    site = s;
                    break;
                }
            }
        }
    }
    TypeNav typeNav = site ? site->typeNav : TypeNav::Other;
    TypeSite typeSite = site ? site->typeSite : TypeSite::Other;
    if (!type.empty()) {
        if (type == "Drake-Class Carrier" || type == "Fleet Carrier") {
            typeNav = TypeNav::Carrier;
            typeSite = TypeSite::FleetCarrier;
        } else if (type == "Squadron Carrier") {
            typeNav = TypeNav::Carrier;
            typeSite = TypeSite::SquadronCarrier;
        } else if (type == "Orbis" || type == "Orbis Starport") {
            typeNav = TypeNav::SpacePort;
            typeSite = TypeSite::Orbis;
        } else if (type == "Ocellus" || type == "Ocellus Starport") {
            typeNav = TypeNav::SpacePort;
            typeSite = TypeSite::Ocellus;
        } else if (type == "Coriolis" || type == "Coriolis Starport") {
            typeNav = TypeNav::SpacePort;
            typeSite = TypeSite::Coriolis;
        } else if (type == "Outpost" || type == "Outpost Starport") {
            typeNav = TypeNav::SpacePort;
            typeSite = TypeSite::SpaceOutpost;
        } else if (type == "SpaceConstr" || type == "SpaceConstructionDepot") {
            typeNav = TypeNav::SpaceConstr;
            typeSite = TypeSite::SpaceConstr;
        }  else if (type == "PlanetConstr" || type == "PlanetaryConstructionDepot") {
            typeNav = TypeNav::PlanetConstr;
            typeSite = TypeSite::PlanetConstr;
        } else if (/*type == "Planetary Outpost" &&*/ sname == "Stronghold Carrier" || sname == "Носитель-база") {
            typeNav = TypeNav::MegashipDock;
            typeSite = TypeSite::StrongholdCarrier;
        } else if (/*type == "Planetary Outpost" &&*/ sname == "Trailblazer Dream") {
            typeNav = TypeNav::MegashipDock;
            typeSite = TypeSite::TrailblazerDream;
        } else if (/*type == "Planetary Outpost" &&*/ sname.starts_with("$EXT_PANEL_ColonisationShip;")) {
            typeNav = TypeNav::MegashipDock;
            typeSite = TypeSite::ColonisationShip;
        } else if (type == "Planetary Outpost") {
            typeNav = TypeNav::PlanetInst;
            typeSite = TypeSite::Other;
        } else if (type == "Settlement" || type == "Odyssey Settlement") {
            typeNav = TypeNav::PlanetPort;
            typeSite = TypeSite::Settlement;
        } else if (type.empty() && sname.starts_with("Orbital Construction Site:")) {
            typeNav = TypeNav::SpaceConstr;
            typeSite = TypeSite::SpaceConstr;
        } else if (type.empty() && sname.starts_with("Planetary Construction Site:")) {
            typeNav = TypeNav::PlanetConstr;
            typeSite = TypeSite::PlanetConstr;
        }
    }
    if (!site) {
        site = std::make_shared<Site>();
        site->marketId = marketId;
        site->name = sname;
        stations.push_back(site);
        saved = false;
    }
    if (!site->marketId && marketId) {
        site->marketId = marketId;
        saved = false;
    }
    if (!type.empty() && site->typeNav != typeNav) {
        site->typeNav = typeNav;
        saved = false;
    }
    if (!type.empty() && site->typeSite != typeSite) {
        site->typeSite = typeSite;
        saved = false;
    }
    if (site->parentBodyId < 0) {
        if (st::space.bodyType == "Planet" || st::space.bodyType == "Star") {
            site->parentBodyId = st::space.bodyId;
            saved = false;
        }
    }
    if (!saved)
        saveStarSystem(this);
    return site;
}

void StarSystem::checkType(spSite& site, gal::TypeNav type, Timestamp timestamp) {
    if (site && site->typeNav != type && site->updated < timestamp) {
        site->typeNav = type;
        site->updated = timestamp;
        saved = false;
    }
}
void StarSystem::checkType(spSite& site, gal::TypeSite type, Timestamp timestamp) {
    if (site && site->typeSite != type && site->updated < timestamp) {
        site->typeSite = type;
        site->updated = timestamp;
        saved = false;
    }
}
void StarSystem::checkName(spSite& site, const std::string& name, Timestamp timestamp) {
    if (site && site->name != name && site->updated < timestamp) {
        if (site->typeSite == TypeSite::FleetCarrier && name.size() == 7 && site->name.ends_with(name))
            return;
        site->name = name;
        site->updated = timestamp;
        saved = false;
    }
}
void StarSystem::checkNloc(spSite& site, const std::string& nloc, Timestamp timestamp) {
    if (site && site->nloc != nloc) {
        site->nloc = nloc;
        site->updated = timestamp;
    }
}
void StarSystem::addFSSSignalDiscovered(std::vector<std::shared_ptr<GameEvent>>& events) {
    if (events.empty())
        return;
    Timestamp timestamp = events.front()->timestamp;
    // get known sites
    std::map<spSite,bool> knownStations;
    for (auto& site : stations) {
        switch(site->typeSite) {
        case TypeSite::Other:
        case TypeSite::EngineerPort:
        case TypeSite::Settlement:
        case TypeSite::SpaceConstr:
        case TypeSite::PlanetConstr:
            continue;
        default:
            knownStations[site] = false;
            continue;
        }
    }
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
        if (data["SignalName_Localised"].is_string())
            snloc = data["SignalName_Localised"].as_string();
        bool isStation = data["IsStation"];

        if (stype == "NavBeacon") {
            sname = "NavBeacon";
        }
        auto site = getDock(sname);
        if (!site) {
            site.reset(new Site());
            site->name = sname;
            if (stype == "FleetCarrier") {
                site->typeNav = TypeNav::Carrier;
                site->typeSite = TypeSite::FleetCarrier;
            }
            stations.push_back(site);
            saved = false;
        }
        else if (site->updated > timestamp)
            continue;
        knownStations[site] = true;
        if (isStation) {
            if (stype == "FleetCarrier") {
                checkType(site, TypeNav::Carrier, timestamp);
                checkType(site, TypeSite::FleetCarrier, timestamp);
            } else if (stype == "StationMegaShip") {
                if (sname == "Stronghold Carrier" || sname == "Носитель-база") {
                    checkType(site, TypeNav::MegashipDock, timestamp);
                    checkType(site, TypeSite::StrongholdCarrier, timestamp);
                    snloc = sname;
                    sname = "Stronghold Carrier";
                } else {
                    checkType(site, TypeNav::MegashipDock, timestamp);
                    checkType(site, TypeSite::StationMegaShip, timestamp);
                }
            } else if (stype == "Megaship") {
                if (sname == "Trailblazer Dream") {
                    checkType(site, TypeNav::MegashipDock, timestamp);
                    checkType(site, TypeSite::TrailblazerDream, timestamp);
                } else {
                    checkType(site, TypeNav::MegashipInst, timestamp);
                    checkType(site, TypeSite::Other, timestamp);
                }
            } else if (stype == "StationONeilOrbis" || stype == "StationONeilCylinder") {
                checkType(site, TypeNav::SpacePort, timestamp);
                checkType(site, TypeSite::Orbis, timestamp);
            } else if (stype == "StationBernalSphere") {
                checkType(site, TypeNav::SpacePort, timestamp);
                checkType(site, TypeSite::Ocellus, timestamp);
            } else if (stype == "StationCoriolis") {
                checkType(site, TypeNav::SpacePort, timestamp);
                checkType(site, TypeSite::Coriolis, timestamp);
            } else if (stype == "AsteroidBase") {
                checkType(site, TypeNav::SpacePort, timestamp);
                checkType(site, TypeSite::AsteroidBase, timestamp);
            } else if (stype == "Outpost") {
                checkType(site, TypeNav::SpacePort, timestamp);
                checkType(site, TypeSite::SpaceOutpost, timestamp);
            }
        } else {
            if (stype == "NavBeacon") {
                checkType(site, TypeNav::Other, timestamp);
                checkType(site, TypeSite::NavBeacon, timestamp);
                sname = "NavBeacon";
            } else if (stype == "Installation") {
                checkType(site, TypeNav::SpaceInst, timestamp);
                checkType(site, TypeSite::Other, timestamp);
            } else if (stype == "StationMegaShip" || stype == "Megaship") {
                if (sname == "Trailblazer Dream") {
                    checkType(site, TypeNav::MegashipDock, timestamp);
                    checkType(site, TypeSite::TrailblazerDream, timestamp);
                } else {
                    checkType(site, TypeNav::MegashipInst, timestamp);
                    checkType(site, TypeSite::Other, timestamp);
                }
            }
        }
        checkName(site, sname, timestamp);
        checkNloc(site, snloc, timestamp);
    }
    //for (auto& it : knownStations) {
    //    if (!it.second && it.first->updated < timestamp) {
    //        std::erase(stations, it.first);
    //        saved = false;
    //    }
    //}
    if (!saved)
        saveStarSystem(this);
}

void setMarketData(spMarket market) {
    auto starSystem = getStarSystem(market->starSystem);
    if (!starSystem)
        return;
    auto dock = starSystem->addStation(market->marketId, market->stationName, market->stationType);
    if (dock->marketData && dock->marketData->timestamp >= market->timestamp)
        return;
    dock->marketData = market;
    starSystem->saved = false;
    saveStarSystem(starSystem.get());
}

bool Item::nameEq(const std::string& nm) {
    return name == nm;
}

bool Site::nameEq(const std::string& nm) {
    if (name == nm)
        return true;
    if (!nloc.empty() && nloc == nm)
        return true;
    if (typeSite == TypeSite::StrongholdCarrier && (nm == "Stronghold Carrier" || nm == "Носитель-база"))
        return true;
    if (typeSite == TypeSite::NavBeacon && (nm == "NavBeacon" || nm == "Нав. маяк"))
        return true;
    if (typeSite == TypeSite::FleetCarrier) {
        if (name.size() == 7 && nm.size() > 7 && nm.ends_with(name))
            return true;
        if (nm.size() == 7 && name.size() > 7 && name.ends_with(nm))
            return true;
    }
    return false;
}

} // namespace gal

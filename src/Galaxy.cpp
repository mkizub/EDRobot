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
    ss->name = jsystem["name"].as_string();
    if (jsystem["address"].is_integer())
        ss->address = jsystem["address"].as_int64();
    else if (jsystem["id64"].is_integer())
        ss->address = jsystem["id64"].as_int64();
    ss->pos.x = jsystem["coords"]["x"].as_number();
    ss->pos.y = jsystem["coords"]["y"].as_number();
    ss->pos.z = jsystem["coords"]["z"].as_number();

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
        //if (!st->nloc.empty())
        //    jsto.emplace("nloc", st->nloc);
        jstations.as_array().push_back(jst);
    }

    json5pp::value jout {
            {"name", ss->name},
            {"address", ss->address},
            {"coords", json5pp::object({
                {"x", ss->pos.x},
                {"y", ss->pos.y},
                {"z", ss->pos.z},
                })},
            {"bodies", jbodies},
            {"stations", jstations},
    };

    std::filesystem::path fp("cache/systems/"+ss->name+".json");
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

    std::ifstream ifs(fp);
    auto jsystem = json5pp::parse5(ifs);
    return fromEDDN(jsystem, true);
}

spStarSystem getStarSystem(const std::string& name) {
    if (gCurrentStarSystem && gCurrentStarSystem->name == name)
        return gCurrentStarSystem;
    const auto& it = gSystemsByNameCache.find(name);
    if (it != gSystemsByNameCache.end() && it->second)
        return it->second;
    spStarSystem ss = loadStarSystem(name);
    if (ss && !ss->saved) {
        assert (ss->name == name);
        gSystemsByNameCache[ss->name] = ss;
        saveStarSystem(ss.get());
    }
    return ss;
}

spStarSystem getStarSystem(const std::string& name, int64_t address) {
    spStarSystem ss = getStarSystem(name);
    if (!ss) {
        ss.reset(new StarSystem());
        ss->name = name;
        ss->address = address;
    }
    return ss;
}

spStarSystem& getCurrentStarSystem() {
    return gCurrentStarSystem;
}
void setCurrentStarSystem(spStarSystem ss) {
    gCurrentStarSystem.swap(ss);
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
    if (this->address != st::destination.system)
        return;
    auto& dname = st::destination.name;
    for (auto& b : this->bodies) {
        if (b->name == dname)
            return;
    }
    for (auto& s : this->stations) {
        if (s->name == dname || s->nloc == dname)
            return;
    }
    if (dname.size() > this->name.size() && dname.starts_with(this->name)) {
        if (dname.size() == this->name.size()+2) {
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
    if (!this->saved)
        saveStarSystem(this);
}

spSite StarSystem::addStation(int64_t marketId, const std::string& sname, const std::string& type) {
    for (auto& s : stations) {
        if (s->marketId == marketId)
            return s;
    }
    spSite site = std::make_shared<Site>();
    site->marketId = marketId;
    site->name = name;
    if (type == "Drake-Class Carrier" || type == "Fleet Carrier") {
        site->typeNav = TypeNav::Carrier;
        site->typeSite = TypeSite::FleetCarrier;
    } else if (type == "Squadron Carrier") {
        site->typeNav = TypeNav::Carrier;
        site->typeSite = TypeSite::SquadronCarrier;
    } else if (type == "Orbis" || type == "Orbis Starport") {
        site->typeNav = TypeNav::SpacePort;
        site->typeSite = TypeSite::Orbis;
    } else if (type == "Ocellus" || type == "Ocellus Starport") {
        site->typeNav = TypeNav::SpacePort;
        site->typeSite = TypeSite::Ocellus;
    } else if (type == "Coriolis" || type == "Coriolis Starport") {
        site->typeNav = TypeNav::SpacePort;
        site->typeSite = TypeSite::Coriolis;
    } else if (type == "Outpost" || type == "Outpost Starport") {
        site->typeNav = TypeNav::SpacePort;
        site->typeSite = TypeSite::SpaceOutpost;
    } else if (type == "SpaceConstr" || type == "SpaceConstructionDepot") {
        site->typeNav = TypeNav::SpaceConstr;
        site->typeSite = TypeSite::SpaceConstr;
    }
    else if (/*type == "Planetary Outpost" &&*/ name == "Stronghold Carrier" || name == "Носитель-база") {
        site->typeNav = TypeNav::MegashipDock;
        site->typeSite = TypeSite::StrongholdCarrier;
    } else if (/*type == "Planetary Outpost" &&*/ name == "Trailblazer Dream") {
        site->typeNav = TypeNav::MegashipDock;
        site->typeSite = TypeSite::TrailblazerDream;
    } else if (/*type == "Planetary Outpost" &&*/ name.starts_with("$EXT_PANEL_ColonisationShip;")) {
        site->typeNav = TypeNav::MegashipDock;
        site->typeSite = TypeSite::ColonisationShip;
    } else if (type == "Planetary Outpost") {
        site->typeNav = TypeNav::PlanetInst;
        site->typeSite = TypeSite::Other;
    } else if (type == "Settlement" || type == "Odyssey Settlement") {
        site->typeNav = TypeNav::PlanetPort;
        site->typeSite = TypeSite::Settlement;
    }
    stations.push_back(site);
    return site;
}

spItem StarSystem::addFSSSignalDiscovered(std::shared_ptr<GameEvent> event) {
    if (event->event != "FSSSignalDiscovered")
        return {};
    json5pp::value& data = event->data;
    if (auto& ja=data["SystemAddress"]; !ja.is_integer() || this->address != ja.as_int64())
        return {};
    std::string stype = data["SignalType"].as_string();
    std::string sname = data["SignalName"].as_string();
    std::string snloc;
    if (data["SignalName_Localised"].is_string())
        snloc = data["SignalName_Localised"].as_string();
    bool isStation = data["IsStation"];
    for (auto& s : this->stations) {
        if (s->typeSite == gal::TypeSite::FleetCarrier && stype == "FleetCarrier") {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
            if (s->name.size() >= 7 && sname.ends_with(s->name.substr(s->name.size() - 7))) {
                s->name = sname;
                saved = false;
                return std::static_pointer_cast<gal::Item>(s);
            }
        }
        else if (s->typeSite == gal::TypeSite::StrongholdCarrier && isStation && stype == "StationMegaShip" && (sname == "Stronghold Carrier" || snloc == "Носитель-база")) {
            return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeSite == gal::TypeSite::Orbis && isStation && (stype == "StationONeilOrbis" || stype == "StationONeilCylinder")) {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeSite == gal::TypeSite::Ocellus && isStation && stype == "StationBernalSphere") {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeSite == gal::TypeSite::Coriolis && isStation && stype == "StationCoriolis") {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeSite == gal::TypeSite::AsteroidBase && isStation && stype == "AsteroidBase") {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeSite == gal::TypeSite::SpaceOutpost && isStation && stype == "Outpost") {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeNav == gal::TypeNav::SpaceInst && !isStation && stype == "Installation") {
            if (s->name == sname)
                return std::static_pointer_cast<gal::Item>(s);
        }
        else if (s->typeSite == gal::TypeSite::NavBeacon && !isStation && stype == "NavBeacon") {
            s->name = "NavBeacon";
            s->nloc = sname;
            return std::static_pointer_cast<gal::Item>(s);
        }
    }
    spSite site(new Site());
    site->name = sname;
    site->nloc = snloc;
    if (isStation) {
        if (stype == "FleetCarrier") {
            site->typeNav = gal::TypeNav::Carrier;
            site->typeSite = gal::TypeSite::FleetCarrier;
        }
        else if (stype == "StationMegaShip") {
            if (sname == "Stronghold Carrier" || sname == "Носитель-база") {
                site->typeNav = gal::TypeNav::MegashipDock;
                site->typeSite = gal::TypeSite::StrongholdCarrier;
                site->name = "Stronghold Carrier";
                site->nloc = "Носитель-база";
            } else {
                site->typeNav = gal::TypeNav::MegashipDock;
                site->typeSite = gal::TypeSite::StationMegaShip;
            }
        }
        else if (stype == "StationONeilOrbis" || stype == "StationONeilCylinder") {
            site->typeNav = gal::TypeNav::SpacePort;
            site->typeSite = gal::TypeSite::Orbis;
        }
        else if (stype == "StationBernalSphere") {
            site->typeNav = gal::TypeNav::SpacePort;
            site->typeSite = gal::TypeSite::Ocellus;
        }
        else if (stype == "StationCoriolis") {
            site->typeNav = gal::TypeNav::SpacePort;
            site->typeSite = gal::TypeSite::Coriolis;
        }
        else if (stype == "AsteroidBase") {
            site->typeNav = gal::TypeNav::SpacePort;
            site->typeSite = gal::TypeSite::AsteroidBase;
        }
        else if (stype == "Outpost") {
            site->typeNav = gal::TypeNav::SpacePort;
            site->typeSite = gal::TypeSite::SpaceOutpost;
        }
    } else {
        if (stype == "NavBeacon") {
            site->typeNav = gal::TypeNav::Other;
            site->typeSite = gal::TypeSite::NavBeacon;
            site->name = "NavBeacon";
            site->nloc = "Нав. маяк";
        }
        else if (stype == "Installation") {
            site->typeNav = gal::TypeNav::SpaceInst;
            site->typeSite = gal::TypeSite::Other;
        }
        else if (stype == "StationMegaShip") {
            site->typeNav = gal::TypeNav::MegashipInst;
            site->typeSite = gal::TypeSite::Other;
        }
    }
    this->stations.push_back(site);
    this->saved = false;
    saveStarSystem(this);
    return site;
}

bool Item::nameEq(const std::string& nm) {
    return name == nm;
}
bool Item::nameEq(const std::wstring& nm) {
    return toUtf16(name) == nm;
}

bool Site::nameEq(const std::string& nm) {
    if (name == nm)
        return true;
    if (!nloc.empty() && nloc == nm)
        return true;
    return false;
}
bool Site::nameEq(const std::wstring& nm) {
    if (toUtf16(name) == nm)
        return true;
    if (!nloc.empty() && toUtf16(nloc) == nm)
        return true;
    return false;
}

} // namespace gal

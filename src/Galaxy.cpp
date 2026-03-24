//
// Created by mkizub on 22.08.2025.
//

#include "pch.h"

#include <unordered_set>

#include "Galaxy.h"
#include "net/EDSM.h"

namespace gal {

std::unordered_map<int64_t,spMarket > gMarketById;
struct CompareStarSystem
{
    using is_transparent = void;
    bool operator()(const spStarSystem& a, const spStarSystem& b) const {return a->systemName < b->systemName;}
    bool operator()(const std::string& a, const spStarSystem& b) const {return a < b->systemName;}
    bool operator()(const spStarSystem& a, const std::string& b) const {return a->systemName < b;}
    bool operator()(std::string_view a, const spStarSystem& b) const {return a < b->systemName;}
    bool operator()(const spStarSystem& a, std::string_view b) const {return a->systemName < b;}
};
std::set<spStarSystem, CompareStarSystem> gSystemsByNameCache;
spStarSystem gCurrentStarSystem = std::make_shared<StarSystem>(0, "Void");

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void saveMarket(Market* market) {
    if (!market || !market->marketId)
        return;

    js::value jm = js::object({
        {"timestamp", formatTimestampString(market->timestamp)},
        {"MarketID", market->marketId},
        {"StationName", market->stationName},
        {"StationType", market->stationType},
        {"StarSystem", market->starSystem},
        {"Items", js::array({})},
        });
    if (market->raven && (!market->raven->buildId.empty() || !market->raven->status.empty())) {
        jm["RavenColonial"] = js::object({
            {"buildId",market->raven->buildId},
            {"status",market->raven->status},
            {"timestamp",formatTimestampString(market->raven->timestamp, true)},
        });
        if (!market->raven->commanders.empty()) {
            auto& jcommanders = jm["RavenColonial"]["commanders"].deref();
            for (auto cmdr: market->raven->commanders) {
                auto &name = cmdr.first;
                auto &ci = cmdr.second;
                auto& jcmdr = jcommanders[name].deref();
                if (ci.timestamp.time_since_epoch().count())
                    jcmdr["timestamp"] = formatTimestampString(ci.timestamp);
                if (ci.deliveries)
                    jcmdr["deliveries"] = ci.deliveries;
                if (ci.contributed)
                    jcmdr["contributed"] = ci.contributed;
                jcmdr.add_flags(js::force::no_indent);
            }
        }
    }
    auto& jarr = jm["Items"].as_array();
    for (auto it : market->items) {
        MarketLine& ml = it.second;
        if (!ml.isConsumer && !ml.isProducer && !ml.stock && !ml.demand)
            continue;
        js::value& jv = jarr.emplace_back(js::object({{"Name", it.first->nameId}}));
        if (ml.stock)
            jv["Stock"] = ml.stock;
        if (ml.demand)
            jv["Demand"] = ml.demand;
        if (ml.buyPrice)
            jv["BuyPrice"] = true;
        if (ml.sellPrice)
            jv["SellPrice"] = ml.sellPrice;
        if (ml.isConsumer)
            jv["Consumer"] = ml.isConsumer;
        if (ml.isProducer)
            jv["Producer"] = true;
        jv.add_flags(js::force::no_indent);
    }

    std::filesystem::path fp("cache/markets/"+std::to_string(market->marketId)+".json");
    std::ofstream ofs(fp);
    ofs << js::rule::ecma404() << js::rule::space_indent<1>() << jm;
    ofs.close();
}

spMarket loadMarket(int64_t marketId) {
    if (!marketId)
        return {};

    const js::value jm = parseJsonFile("cache/markets/"+std::to_string(marketId)+".json");

    Timestamp timestamp;
    if (!parseTimestamp(jm["timestamp"], timestamp))
        return {};
    if (marketId != jm["MarketID"].as_int_or())
        return {};
    spMarket market = std::make_shared<Market>(Market{
        .timestamp = timestamp,
        .marketId = marketId,
        .stationName = jm["StationName"].as_string_or(),
        .stationType = jm["StationType"].as_string_or(),
        .starSystem = jm["StarSystem"].as_string_or(),
    });
    if (jm["RavenColonial"].is_object()) {
        market->raven = std::make_shared<RavenProj>();
        market->raven->buildId = jm["RavenColonial"]["buildId"].as_string_or();
        market->raven->status = jm["RavenColonial"]["status"].as_string_or();
        parseTimestamp(jm["RavenColonial"]["timestamp"], market->raven->timestamp);
        if (auto commanders=jm["RavenColonial"]["commanders"]; commanders.is_object())
        for (auto [name,jcmdr] : commanders.key_value()) {
            RavenProj::CmdrInfo ci {};
            if (!jcmdr["timestamp"].empty())
                parseTimestamp(jcmdr["timestamp"], ci.timestamp);
            ci.deliveries = jcmdr["deliveries"].as_int_or();
            ci.contributed = jcmdr["contributed"].as_int_or();
            market->raven->commanders.emplace(name, ci);
        }
    }
    auto& items = jm["Items"].as_array();
    for (auto it : items) {
        Commodity* commodity = Cfg.getCommodityById(it["Name"].as_string());
        if (!commodity)
            continue;
        MarketLine ml {};
        ml.buyPrice = it["BuyPrice"].as_int_or();
        ml.sellPrice = it["SellPrice"].as_int_or();
        ml.stock = it["Stock"].as_int_or();
        ml.demand = it["Demand"].as_int_or();
        ml.isConsumer = it["Consumer"].as_bool_or();
        ml.isProducer = it["Producer"].as_bool_or();
        market->items.emplace(commodity, ml);
    }

    return market;
}

static void parseUpdated(spEntity& entity, const js::value& j) {
    auto upd = j["updateTime"];
    if (upd.is_int()) {
        std::chrono::seconds sec_duration(upd.as_int());
        std::chrono::sys_time<std::chrono::seconds> sys_time(sec_duration);
        entity->updated = std::chrono::utc_clock::from_sys(sys_time);
    }
    else if (upd.is_string()) {
        parseTimestampString(upd.as_string(), entity->updated);
    }
}
static void parseBodyId(const spStarSystem& ss, spEntity& entity, const js::value& j) {
    if (j.at("bodyId").is_int())
        entity->bodyId = j.at("bodyId").as_int();
    if (j["parentBodyId"].is_int())
        entity->parentBodyId = j["parentBodyId"].as_int();
    else if (j["parents"].is_array() && !j["parents"].as_array().empty()) {
        auto& jp = j["parents"].as_array()[0];
        if (jp.is_object())
            entity->parentBodyId = jp.key_value().begin().value().as_int();
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
static spStarSystem fromEDDN(const js::value& jsystem, bool saved) {
    auto systemName = jsystem["name"].as_string();
    auto systemAddress = jsystem["address"].exists()
            ? jsystem["address"].as_int()
            : jsystem["id64"].as_int();
    auto ss_it = gSystemsByNameCache.find(systemName);
    spStarSystem ss = ss_it != gSystemsByNameCache.end() ? *ss_it
            : spStarSystem(new StarSystem(systemAddress, systemName));
    ss->starPos.x = jsystem["coords"]["x"].as_real_or();
    ss->starPos.y = jsystem["coords"]["y"].as_real_or();
    ss->starPos.z = jsystem["coords"]["z"].as_real_or();

    for (auto& jb : jsystem["bodies"].as_array_or()) {
        spEntity body(new Entity);
        if (jb["type"].is_string()) {
            std::string type = jb["type"].as_string();
            if (auto typeNav = enum_cast<TypeNav>(type); typeNav.has_value())
                body->type = typeNav.value();
        }
        parseUpdated(body, jb);
        parseBodyId(ss, body, jb);

        if (jb["name"].is_string()) {
            auto& name = jb["name"].as_string();
            if (ss->getBody(name))
                continue;
            body->setName(name);
        }
        if (jb["distanceToArrival"].is_number())
            body->main_star_distance = dist_t(dist_t::LS, jb["distanceToArrival"].as_real());
        if (body->type == TypeNav::Star) {
            if (jb["radius"].is_number())
                body->radius = jb["radius"].as_real(); // KM
            else if (jb["solarRadius"].is_number())
                body->radius = jb["solarRadius"].as_real() * 6.957e5; // KM
            if (jb["spectralClass"].is_string())
                body->code = jb["spectralClass"].as_string();
            body->special = jb["isMainStar"].as_bool_or();
        }
        else if (body->type == TypeNav::Planet) {
            body->radius = jb["radius"].as_real_or(); // KM
            body->special = jb["isLandable"].as_bool_or();
        }
        ss->bodies.push_back(body);
    }

    //std::string lng = toLower(std::string(enum_name<Lang>(Cfg.lng)));
    for (auto& jb : jsystem["stations"].as_array_or()) {
        std::string name = jb["name"].as_string_or();
        if (ss->getDock(name))
            continue;
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
        if (auto* nt = NavType::findNavType(site->type); nt && !nt->name_pattern && !nt->name_loc.empty())
            site->nloc = nt->get_nloc();

        if (jb["marketId"].is_int())
            site->marketId = jb["marketId"].as_int();
        else if (jb["id64"].is_int())
            site->marketId = jb["id64"].as_int();
        ss->stations.push_back(site);
    }

    ss->saved = saved;
    gSystemsByNameCache.insert(ss);
    return ss;
}

void StarSystem::save() {
    std::vector sorted_bodies = bodies;
    std::sort(sorted_bodies.begin(), sorted_bodies.end(), [](const spEntity& a, const spEntity& b) {
        return a->bodyId < b->bodyId;
    });
    js::value jbodies = js::array({});
    js::value jstations = js::array({});
    for (auto& body : sorted_bodies) {
        js::value jb {
                {"type", std::string(enum_name<TypeNav>(body->type))},
        };
        if (body->type != TypeNav::Barycenter && !body->name.empty())
            jb["name"] = body->name;
        if (body->bodyId >= 0)
            jb["bodyId"] = body->bodyId;
        if (body->parentBodyId >= 0)
            jb["parentBodyId"] = body->parentBodyId;
        if (body->main_star_distance.valid())
            jb["distanceToArrival"] = std::round(body->main_star_distance.get_ls()*10.0)/10.0;
        if (body->radius)
            jb["radius"] = std::round(body->radius*1000.0)/1000.0;
        if (body->type == TypeNav::Star) {
            if (body->special)
                jb["isMainStar"] = true;
            if (!body->code.empty())
                jb["spectralClass"] = body->code;
        }
        else if (body->type == TypeNav::Planet) {
            if (body->special)
                jb["isLandable"] = true;
            if (!body->code.empty())
                jb["spectralClass"] = body->code;
        }

        {
            auto ts = std::chrono::floor<std::chrono::seconds>(body->updated);
            auto seconds = ts.time_since_epoch().count();
            if (seconds)
                jb["updateTime"] = seconds;
        }
        jb.add_flags(js::force::no_indent);
        jbodies.as_array().push_back(jb);
    }
    for (auto& st : stations) {
        js::value jst {
                {"type", std::string(enum_name<TypeNav>(st->type))},
                {"name", st->name},
        };
        if (st->bodyId >= 0)
            jst["bodyId"] = st->bodyId;
        if (st->parentBodyId >= 0)
            jst["parentBodyId"] = st->parentBodyId;
        if (st->main_star_distance.valid())
            jst["distanceToArrival"] = std::round(st->main_star_distance.get_ls()*10.0)/10.0;
        if (st->marketId)
            jst["marketId"] = st->marketId;
        {
            auto ts = std::chrono::floor<std::chrono::seconds>(st->updated);
            auto seconds = ts.time_since_epoch().count();
            if (seconds)
                jst["updateTime"] = seconds;
        }
        jst.add_flags(js::force::no_indent);
        jstations.as_array().push_back(jst);
    }

    js::value jout {
            {"name",     systemName},
            {"address",  systemAddress},
            {"coords",   js::object({
                {"x", starPos.x},
                {"y", starPos.y},
                {"z", starPos.z},
                })},
            {"bodies",   jbodies},
            {"stations", jstations},
    };
    jout["coords"].add_flags(js::force::no_indent);

    std::filesystem::path fp("cache/systems/"+systemName+".json");
    std::ofstream ofs(fp);
    ofs << std::setprecision(15) << std::defaultfloat << js::rule::ecma404() << js::rule::space_indent<1>() << jout;
    ofs.close();

    saved = true;
}

static spStarSystem loadStarSystem(std::string name) {
    spStarSystem ss;
    std::filesystem::path fp("cache/systems/"+name+".json");
    if (std::filesystem::exists(fp)) {
        auto jsystem = parseJsonFile(fp.string());
        if (!jsystem.empty())
            ss = fromEDDN(jsystem, true);
    }
    if (!ss || ss->bodies.empty()) {
        auto jsystem = EDSM::loadStarSystem(name);
        if (!jsystem.empty())
            ss = fromEDDN(jsystem, false);
    }
    return ss;
}

spStarSystem getStarSystem(std::string_view name) {
    spStarSystem ss;
    if (gCurrentStarSystem && gCurrentStarSystem->systemName == name) {
        ss = gCurrentStarSystem;
    }
    else if (const auto& it = gSystemsByNameCache.find(name); it != gSystemsByNameCache.end()) {
        ss = *it;
    }
    if (!ss || ss->bodies.empty())
        ss = loadStarSystem(std::string(name));
    if (ss && !ss->saved) {
        assert (ss->systemName == name);
        ss->save();
    }
    return ss;
}

spStarSystem makeStarSystem(const std::string& name, int64_t address) {
    spStarSystem ss = getStarSystem(name);
    if (!ss) {
        ss.reset(new StarSystem(address, name));
        gSystemsByNameCache.insert(ss);
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
    if (bodyId < 0)
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
    std::string xx_name;
    std::string ru_name;
    if (NavType::expandName(sname, xx_name, ru_name)) {
        for (auto& s : this->stations) {
            if (s->nameEq(xx_name) || s->nameEq(ru_name))
                return s;
        }
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
    std::string xx_name;
    std::string ru_name;
    bool exp = NavType::expandName(dname, xx_name, ru_name);
    for (auto& s : this->stations) {
        if (s->nameEq(dname) || (exp && s->nameEq(xx_name)) || (exp && s->nameEq(ru_name))) {
            switch (s->type) {
            case TypeNav::FleetCarrier:
                if (s->name != dname) {
                    s->setName(dname);
                    this->saved = false;
                }
                // fall through
            case TypeNav::NavBeacon:
            case TypeNav::TouristBeacon:
            case TypeNav::SpaceInstallation:
            case TypeNav::SquadronCarrier:
            case TypeNav::StrongholdCarrier:
            case TypeNav::ColonisationShip:
            case TypeNav::Megaship:
            //case TypeNav::TrailblazerDream:
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
                }
                if (!this->saved) {
                    save();
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
    else if (dname.starts_with("$EXT_PANEL_ColonisationShip;")) {
        spEntity site = std::make_shared<Entity>();
        site->type = TypeNav::ColonisationShip;
        site->setName(dname);
        site->parentBodyId = st::destination.bodyId;
        stations.push_back(site);
        this->saved = false;
    }
    if (!this->saved)
        save();
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
                if (tp == TypeNav::Other)
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
        save();
    return entity;
}

spEntity StarSystem::addStation(spGameEvent& ge) {
    if (!(ge->event=="ApproachSettlement" || ge->event=="Docked" || (ge->event=="Location") && ge->data["Docked"]))
        return {};
    auto& je = ge->data;

    std::string sname;
    std::string stype;
    int64_t marketId = je["MarketID"].as_int_or();
    if (ge->event=="ApproachSettlement") {
        sname = st::space.stationName;
        stype = st::space.stationType;
    } else {
        sname = je.at("StationName","").as_string();
        stype = je.at("StationType","").as_string();
    }

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
        stations.push_back(dock);
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
                if (nt->type == dock->type || dock->type == TypeNav::Other) {
                    typeNav = nt->type;
                    break;
                }
                tp = nt->type;
            }
        }
        if (tp != TypeNav::Other)
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
    if (!sname.empty() && (dock->name.empty() || !dock->nameEq(sname))) {
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
        double dist = je["DistFromStarLS"].as_real_or();
        if (!dock->main_star_distance || std::round(dock->main_star_distance.get_ls()) != std::round(dist)) {
            dock->main_star_distance = dist_t(dist_t::LS, dist);
            saved = false;
        }
    }
    if (!saved)
        save();
    return dock;
}

spEntity StarSystem::addStation(spEntity station) {
    if (station) {
        stations.push_back(station);
    }
    return station;
}

spEntity StarSystem::addSignal(spEntity signal) {
    if (signal) {
        assert (isSignal(signal->type));
        signals.push_back(signal);
    }
    return signal;
}

void StarSystem::checkType(spEntity& site, TypeNav type, Timestamp timestamp) {
    if (site && /*type != TypeNav::Other &&*/ site->type != type && site->updated < timestamp) {
        site->type = type;
        site->updated = timestamp;
        saved = false;
    }
}
void StarSystem::checkName(spEntity& site, const std::string& name, Timestamp timestamp) {
    if (site && site->updated < timestamp) {
        if (site->type == TypeNav::FleetCarrier) {
            if (site->name != name && site->setName(name)) {
                site->updated = timestamp;
                saved = false;
            }
        }
        else if (!site->nameEq(name) && site->setName(name)) {
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
void StarSystem::addFSSSignalDiscovered(const std::vector<std::shared_ptr<GameEvent>>& events) {
    if (events.empty())
        return;
    Timestamp timestamp = events.front()->timestamp;
    // add all sites from events
    for (auto event : events) {
        if (event->event != "FSSSignalDiscovered")
            return;
        auto& data = event->data;
        if (this->systemAddress != data["SystemAddress"].as_int_or())
            return;
        std::string stype = data["SignalType"].as_string();
        std::string sname = data["SignalName"].as_string();
        std::string snloc;
        if (data["SignalName_Localised"].is_string()) {
            snloc = data["SignalName_Localised"].as_string();
            sname = snloc;
        }
        bool isStation = bool(data["IsStation"]);

        spEntity site = getDock(sname);
        if (!site) {
            site.reset(new Entity());
            if (stype == "NavBeacon")
                site->type = TypeNav::NavBeacon;
            else if (stype == "FleetCarrier")
                site->type = TypeNav::FleetCarrier;
            else if (stype == "SquadronCarrier")
                site->type = TypeNav::SquadronCarrier;
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
            for (auto nt: ALL_NAV_TYPES) {
                if (nt->match_type(stype)) {
                    typeNav = nt->type;
                    break;
                }
            }
        }
        if (snloc.empty()) {
            if (auto* nt = NavType::findNavType(site->type); nt && !nt->name_loc.empty())
                snloc = nt->get_nloc();
        }
        checkType(site, typeNav, timestamp);
        checkName(site, sname, timestamp);
        checkNloc(site, snloc, timestamp);
    }
    if (!saved)
        save();
}

spMarket getMarket(int64_t marketId) {
    if (!marketId)
        return {};
    auto market = gMarketById[marketId];
    if (market && !market->items.empty())
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
    if (nm.empty())
        return false;
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
    case TypeNav::Dodec:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceInstallation:
    case TypeNav::SquadronCarrier:
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
    case TypeNav::ColonisationShip: {
        if (name == nm || nloc == nm)
            return true;
        std::string xx_name;
        std::string ru_name;
        if (NavType::expandName(nm, xx_name, ru_name)) {
            if (name == xx_name || nloc == ru_name)
                return true;
        }
        return false;
    }
    //case TypeNav::TrailblazerDream:
    //    navType = &TRAILBLAZER_DREAM;
    }
    for (auto& p : navType->name_loc) {
        if (p.second == nm)
            return true;
    }
    return false;
}

bool Entity::setName(const std::string& nm) {
    std::string xx_name;
    std::string ru_name;
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
    case TypeNav::Dodec:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceOutpost:
    case TypeNav::SpaceInstallation:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::Megaship:
    case TypeNav::StationMegaShip:
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
    case TypeNav::SquadronCarrier:
        if (name == nm)
            return false;
        if (nm.size() > 7 && nm.substr(nm.size()-7, 3) == " | ")
            code = nm.substr(nm.size()-4);
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
    //case TypeNav::TrailblazerDream:
    //    navType = &TRAILBLAZER_DREAM;
    //    break;
    case TypeNav::ColonisationShip:
        if (NavType::expandName(nm, xx_name, ru_name)) {
            if (name == xx_name && nloc == ru_name)
                return false;
            name = xx_name;
            nloc = ru_name;
            return true;
        }
        for (auto& nl : gal::COLONIZATION_SHIP.name_loc) {
            if (nl.first == Lang::XX)
                xx_name = nl.second;
            if (nl.first == Lang::EN)
                xx_name = nl.second;
            if (nl.first == st::lng)
                ru_name = nl.second;
        }
        if (ru_name.empty())
            ru_name = xx_name;
        for (auto& nl : gal::COLONIZATION_SHIP.name_loc) {
            if (nm.starts_with(nl.second)) {
                std::string own_name = trim(nm.substr(nl.second.size()));
                if (!own_name.empty()) {
                    xx_name += " " + own_name;
                    ru_name += " " + own_name;
                }
                if (name == xx_name && nloc == ru_name)
                    return false;
                name = xx_name;
                nloc = ru_name;
                return true;
            }
        }
        return true;
    }

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

//
// Created by mkizub on 21.09.2026.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include "Galaxy.h"
#include "../db/DB.h"
#include "../net/RavenColonial.h"

namespace gal {

static void update_body_id_and_parents(gal::StarSystem* ss, gal::spEntity &entity, db::BodyJS& db_body) {
    if (db_body.bodyId.has_value() && !entity->bodyId.has_value()) {
        entity->bodyId = db_body.bodyId;
        ss->needBlobSave = true;
    }

    if (!db_body.parents.empty()) {
        entity->parentBodyId = db_body.parents[0].bodyId;
        auto b = entity;
        for (const auto &jp: db_body.parents) {
            TypeNav p_type = toTypeNav(jp.type);
            body_id_t p_id;
            p_id = jp.bodyId;
            if (!isBody(p_type) || !p_id.has_value())
                break;
            if (b && b->parentBodyId != p_id) {
                b->parentBodyId = p_id;
                ss->needBlobSave = true;
            }
            auto p = ss->getBodyById(p_id);
            //if (!p) {
            //    std::string name;
            //    if (p_type == TypeNav::Star)
            //        name = std::format("\tStar#{}", p_id.val);
            //    else if (p_type == TypeNav::Planet)
            //        name = std::format("\tPlanet#{}", p_id.val);
            //    p = ss->addEntity(p_type, name, "", {.body_id=p_id, .ts=db_body.updated_at});
            //    p->bodyId = p_id;
            //}
            if (!p && (p_type == TypeNav::Barycenter || p_type == TypeNav::Ring))
                p = ss->addEntity(p_type, "", "", {.body_id=p_id, .ts=db_body.updated_at});
            b = p;
        }
    }
}


template <typename T>
concept withHasValue = requires (T t) { { t.has_value() } -> std::same_as<bool>; };

template <typename T>
concept withEmpty = requires (T t) { { t.empty() } -> std::same_as<bool>; };

template <typename T>
concept Optional = withHasValue<T> || withEmpty<T> || std::is_convertible_v<T,bool>;


template <Optional T>
void merge(StarSystem* ss, T& lhs, T& rhs) {
    bool patch = false;
    if constexpr (std::is_same_v<LandingPads, T>) {
        patch = rhs.has_value() && (lhs.large != rhs.large || lhs.medium != rhs.medium || lhs.small != rhs.small);
    }
    else if constexpr (std::is_same_v<js::symbol, T>) {
        patch = rhs.has_value() && (!lhs.has_value() || rhs.sv() != lhs.sv());
    }
    else if constexpr (std::is_same_v<opt_float, T>) {
        patch = rhs.has_value() && (!lhs.has_value() || rhs.value() != lhs.value());
    }
    else if constexpr (std::is_same_v<opt_bool, T>) {
        patch = rhs.has_value() && (!lhs.has_value() || rhs.value() != lhs.value());
    }
    else if constexpr (withHasValue<T>) {
        patch = rhs.has_value() && (!lhs.has_value() || lhs.value() != rhs.value());
    }
    else if constexpr (withEmpty<T>) {
        patch = !rhs.empty() && (lhs.empty() || lhs.value() != rhs.value());
    }
    else if constexpr (std::is_integral_v<T>) {
        patch = rhs != 0 && lhs != rhs;
    }
    else {
        patch = !lhs;
    }

    if (patch) {
        lhs = rhs;
        ss->needBlobSave = true;
    }
}

template <typename K, typename V>
void merge(StarSystem* ss, js::small_map<K,V>& lhs, js::small_map<K,V>& rhs) {
    if (lhs != rhs) {
        lhs = rhs;
        ss->needBlobSave = true;
    }
}

template <typename V>
void merge(StarSystem* ss, js::vector<V>& lhs, js::vector<V>& rhs) {
    if (lhs != rhs) {
        lhs = rhs;
        ss->needBlobSave = true;
    }
}

#define MERGE(to, from, fld) merge(ss, (to).fld, (from).fld)

bool update_market(StarSystem* ss, db::StationJS& db_st) {
    if (!db_st.marketId || !db_st.market)
        return false;
    spMarket old_market = gal::getMarket(db_st.marketId);
    if (old_market && old_market->timestamp >= db_st.market->updated)
        return false;

    std::string stationType;
    TypeNav tn = toTypeNav(db_st.type);
    if (tn == TypeNav::SpaceConstrDepot || tn == TypeNav::PlanetaryConstrDepot || tn == TypeNav::ColonisationShip)
        stationType = "ConstrDepot";
    else
        stationType = *db_st.type.sv();
    spMarket market = std::make_shared<Market>(Market{
            .timestamp = db_st.market->updated,
            .marketId = db_st.marketId,
            .systemAddress = ss->systemAddress,
            .stationName = db_st.name,
            .stationType = stationType,
            .starSystem = ss->systemName,
    });

    if (old_market)
        market->raven = old_market->raven;

    market->items.reserve(db_st.market->commodities.size());
    for (auto& ln : db_st.market->commodities) {
        Commodity* commodity = Cfg.getCommodityById(ln.commodityId);
        if (!commodity)
            continue;
        MarketLine ml {
            .buyPrice = (int)ln.buyPrice,
            .sellPrice = (int)ln.sellPrice,
            .stock = (int)ln.supply,
            .demand = (int)ln.demand,
            .isConsumer = ln.isConsumer,
            .isProducer = ln.isProducer,
        };
        market->items[commodity] = ml;
    }

    gal::setMarketData(market);
    return true;
}

bool update_signal(StarSystem* ss, db::SignalJS& db_sig) {
    TypeNav tp = toTypeNav(db_sig.type);
    if (!isSignal(tp))
        return false;

    spEntity sig = ss->getSignal(db_sig.name);

    if (!sig) {
        if (!db_sig.var.empty())
            ss->addEntity(tp, db_sig.var, "", {});
        else
            ss->addEntity(tp, db_sig.name, "", {});
    }
    else if (sig->type != tp) {
        LOG_ERROR("Missmatch signal type: old {}, net/db {} for signal '{}'",
                  enum_name(sig->type), enum_name(tp), db_sig.name);
        if (!db_sig.var.empty())
            ss->checkTypeAndName(sig, tp, db_sig.var, db_sig.name, {.ts=sig->updated});
        else
            ss->checkTypeAndName(sig, tp, db_sig.name, "", {.ts=sig->updated});
    }
    else if (!db_sig.var.empty() && db_sig.var != sig->getUnexpandedName()) {
        ss->checkTypeAndName(sig, tp, db_sig.var, db_sig.name, {.ts=sig->updated});
    }
    else if (sig->name.empty()) {
        LOG_ERROR("Empty signal name for type: {}", enum_name(sig->type));
        ss->checkTypeAndName(sig, tp, db_sig.name, "", {.ts=sig->updated});
    }

    return true;
}

bool update_station(StarSystem* ss, db::StationJS& db_st, spEntity& st) {
    TypeNav tp = TypeNav::SpaceStation;
    if (db_st.latitude.has_value() || db_st.longitude.has_value())
        tp = TypeNav::PlanetaryThing;
    if (db_st.type.has_value())
        tp = toTypeNav(db_st.type);

    if (tp == TypeNav::PlanetaryInstallation && db_st.name == "Stronghold Carrier")
        tp = TypeNav::AnonMegaship;
    if (tp == TypeNav::PlanetaryThing && db_st.latitude.has_value())
        tp = TypeNav::PlanetaryInstallation;

    if (tp < TypeNav::SpaceStation)
        return false;

    if (!st && db_st.marketId)
        st = ss->getDock(db_st.marketId);
    if (!st)
        st = ss->getDock(db_st.name);
    if (!st)
        st = ss->getBodyById(db_st.bodyId);

    if (!st) {
        if (!db_st.var.empty())
            st = ss->addEntity(tp, db_st.var, db_st.name, {.market_id=db_st.marketId, .ts=db_st.updated_at});
        else
            st = ss->addEntity(tp, db_st.name, "", {.market_id=db_st.marketId, .ts=db_st.updated_at});
        st->bodyId = db_st.bodyId;
        if (db_st.marketId != st->marketId())
            st->setMarketId(db_st.marketId);
    }
    else if (st->type != tp) {
        st = ss->checkTypeAndName(st, tp, db_st.name, "", {.market_id=st->marketId(), .ts=st->updated});
    }
    if (db_st.var != st->getUnexpandedName()) {
        st = ss->checkTypeAndName(st, tp, db_st.var, db_st.name, {.market_id=st->marketId(), .ts=st->updated});
    }
    if (st->name.empty()) {
        LOG_ERROR("Missing name for station '{}' in '{}'", enum_name(st->type), ss->systemName);
        if (!db_st.var.empty())
            st = ss->checkTypeAndName(st, tp, db_st.var, db_st.name, {.market_id=st->marketId(), .ts=st->updated});
        else
            st = ss->checkTypeAndName(st, tp, db_st.name, "", {.market_id=st->marketId(), .ts=st->updated});
    }
    if (db_st.bodyId.has_value() && db_st.bodyId != st->bodyId) {
        if (!st->bodyId.has_value()) {
            st->bodyId = db_st.bodyId;
            st->updated = db_st.updated_at;
        } else {
            LOG_ERROR("Missmatch body id {} != {} for station '{}' in '{}'",
                      st->bodyId.val, db_st.bodyId.val, enum_name(st->type), ss->systemName);
        }
    }
    if (db_st.parentId.has_value() && st->parentBodyId != db_st.parentId) {
        st->parentBodyId = db_st.parentId;
        st->updated = db_st.updated_at;
    }
    if (db_st.marketId > 0 && db_st.marketId != st->marketId()) {
        if (st->marketId() <= 0) {
            st->setMarketId(db_st.marketId);
            st->updated = db_st.updated_at;
        } else {
            LOG_ERROR("Missmatch marketId id {} != {} for station '{}' in '{}'",
                      st->marketId(), db_st.marketId, enum_name(st->type), ss->systemName);
        }
    }

    {
        auto* ext = st->getStationData();
        assert (ext);
        if (!ext)
            return false;

        // clear values that are always the same
        if (st->type == TypeNav::FleetCarrier) {
            ext->controllingFaction.clear();
            ext->economies.clear();
        }
        if (st->type == TypeNav::SquadronCarrier) {
            ext->economies.clear();
        }

        MERGE(*ext, db_st, controllingFaction);
        MERGE(*ext, db_st, allegiance);
        MERGE(*ext, db_st, government);
        MERGE(*ext, db_st, state);
        MERGE(*ext, db_st, controllingFactionState);
        MERGE(*ext, db_st, primaryEconomy);
        MERGE(*ext, db_st, carrierDockingAccess);
        MERGE(*ext, db_st, economies);
        MERGE(*ext, db_st, distanceToArrival);
        MERGE(*ext, db_st, latitude);
        MERGE(*ext, db_st, longitude);
        MERGE(*ext, db_st, landingPads);
        MERGE(*ext, db_st, services);
    }

    update_market(ss, db_st);

    return true;
}

bool update_body(StarSystem* ss, db::BodyJS& db_body) {
    if (!db_body.bodyId.has_value())
        return false;

    TypeNav tp = toTypeNav(db_body.type);
    if (!isBody(tp))
        return false;

    spEntity body;

    body = ss->getBodyById(db_body.bodyId);
    if (!body) {
        body = ss->addEntity(tp, db_body.name, "", {.body_id=db_body.bodyId, .ts=db_body.updated_at});
        body->bodyId = db_body.bodyId;
    } else if (body->type != tp) {
        if (isSignal(body->type) || body->type == TypeNav::Body)
            body = ss->checkTypeAndName(body, tp, db_body.name, "",
                                        {.body_id=db_body.bodyId, .ts=body->updated});
    }
    if (body->name.empty() && !db_body.name.empty()) {
        body = ss->checkTypeAndName(body, body->type, db_body.name, "",
                                    {.body_id=db_body.bodyId, .ts=body->updated});
    }

    update_body_id_and_parents(ss, body, db_body);

    {
        auto* ext = body->getBodyData();
        assert (ext);
        if (!ext)
            return false;
        MERGE(*ext, db_body, tidalLock);
        MERGE(*ext, db_body, radius);
        MERGE(*ext, db_body, distanceToArrival);
        MERGE(*ext, db_body, rotationalPeriod);
        MERGE(*ext, db_body, orbitalPeriod);
        MERGE(*ext, db_body, semiMajorAxis);
        MERGE(*ext, db_body, orbitalEccentricity);
        MERGE(*ext, db_body, orbitalInclination);
        MERGE(*ext, db_body, argOfPeriapsis);
        MERGE(*ext, db_body, meanAnomaly);
        MERGE(*ext, db_body, ascendingNode);
        MERGE(*ext, db_body, surfaceTemperature);
        MERGE(*ext, db_body, axialTilt);
    }
    if (body->type == TypeNav::Star && db_body.getStarPart()) {
        auto* sp = db_body.getStarPart();
        auto* ext = body->getStarData();
        assert (ext);
        if (ext && sp) {
            MERGE(*ext, *sp, mainStar);
            if (sp->subClass >= 0 && ext->subClass != sp->subClass) {
                ext->subClass = sp->subClass;
            }
            MERGE(*ext, *sp, luminosity);
            MERGE(*ext, *sp, age);
            MERGE(*ext, *sp, solarMasses);
            MERGE(*ext, *sp, absoluteMagnitude);
        }
    }
    if (body->type == TypeNav::Planet && db_body.getPlanetPart()) {
        auto* sp = db_body.getPlanetPart();
        auto* ext = body->getPlanetData();
        assert (ext);
        if (ext && sp) {
            MERGE(*ext, *sp, isLandable);
            MERGE(*ext, *sp, volcanismType);
            MERGE(*ext, *sp, atmosphereType);
            MERGE(*ext, *sp, terraformingState);
            MERGE(*ext, *sp, reserveLevel);
            MERGE(*ext, *sp, earthMasses);
            MERGE(*ext, *sp, surfaceGravity);
            MERGE(*ext, *sp, surfacePressure);
        }
    }

    for (auto& db_st : db_body.stations) {
        db_st.parentId = db_body.bodyId;
        spEntity station;
        update_station(ss, db_st, station);
    }

    return true;
}

bool update_star_system(StarSystem* ss, db::StarSystemJS& db_ss) {
    if (!ss)
        return false;

    Coords systemPos{db_ss.coords.x, db_ss.coords.y, db_ss.coords.z};
    if (!systemPos.empty() && ss->systemCoords != systemPos) {
        const_cast<Coords&>(ss->systemCoords) = systemPos;
        ss->needCoreSave = true;
    }
    if (db_ss.updated_at > ss->updated_at) {
        ss->updated_at = db_ss.updated_at;
        ss->needCoreSave = true;
    }
    if (db_ss.updated_at > ss->eddn_updated_at) {
        ss->eddn_updated_at = db_ss.updated_at;
        ss->needCoreSave = true;
    }

    // update StarSystem data
    /*if (ss->needCoreSave)*/ {
        ss->needBlobSave = true;
        ss->updated_at = db_ss.updated_at;

        gal::StarSystem::Extra &ext = ss->ext;
        MERGE(ext, db_ss, allegiance);
        MERGE(ext, db_ss, government);
        MERGE(ext, db_ss, primaryEconomy);
        MERGE(ext, db_ss, secondaryEconomy);
        MERGE(ext, db_ss, security);
        if (db_ss.bodyCount) { MERGE(ext, db_ss, bodyCount); }
        if (db_ss.population > ext.population) { MERGE(ext, db_ss, population); }
    }

    // update bodies
    for (auto& db_body : db_ss.bodies) {
        update_body(ss, db_body);
    }

    for (auto& db_st : db_ss.stations) {
        spEntity station;
        update_station(ss, db_st, station);
    }

    for (auto& db_sig : db_ss.signals) {
        update_signal(ss, db_sig);
    }

    return true;
}

bool update_megaship(StarSystem* ss, db::MegashipJS& db_ms, spEntity& megaship) {
    if (!ss || db_ms.systemId != ss->systemAddress)
        return {};
    TypeNav type = toTypeNav(db_ms.blob.type);
    if (!isMegaship(type))
        return {};
    if (db_ms.marketId > 0 && db_ms.blob.marketId <= 0)
        db_ms.blob.marketId = db_ms.marketId;
    if (!db_ms.name.empty() && db_ms.blob.name.empty())
        db_ms.blob.name = db_ms.name;
    if (!db_ms.code.empty() && db_ms.blob.name.empty())
        db_ms.blob.name = db_ms.code;
    if (*db_ms.updated && !db_ms.blob.updated_at)
        db_ms.blob.updated_at = db_ms.updated;
    return update_station(ss, db_ms.blob, megaship);
}


js::EnumVal* badEnum(js::EnumDeclBase& decl, std::string_view sv) {
    LOG_WARNING("Bad enum value \"{}\" for {}", sv, decl.name);
    return decl.addNewValue(*sv);
}

void updateFromFSSSignalDiscovered(StarSystem* ss, const std::vector<spGameEvent>& events) {
    if (!ss || !ss->systemAddress) {
        LOG_ERROR("dummy StarSystem::addFSSSignalDiscovered()");
        return;
    }
    if (events.empty())
        return;
    Timestamp timestamp = events.front()->timestamp;
    // add all sites from events
    for (auto ge : events) {
        try {
        if (ge->event != "FSSSignalDiscovered")
            return;
        auto& data = ge->data;
        if (ss->systemAddress != data["SystemAddress"].as_int_or())
            return;
        const auto stype = data["SignalType"].as_string_or();
        std::string_view sname = data["SignalName"].as_string();
        std::string_view snloc = data["SignalName_Localised"].as_string_or();
        bool is_station = data["IsStation"].as_bool_or();

        TypeNav tn = toTypeNav(JsNavType::get(stype));
        if (tn == TypeNav::Other && data["USSType"].is_string())
            tn = TypeNav::USS;
        else
            tn = NavType::guessTypeFromName(tn, sname);

        if (tn == TypeNav::USS) {
            continue;
        }

        spEntity site;
        if (isAnySite(tn) || is_station) {
            site = ss->getDock(sname);
            if (!site)
                site = ss->getDock(snloc);
            if (!site)
                site = ss->getSignal(sname);
        } else {
            site = ss->getSignal(sname);
            if (!site)
                site = ss->getDock(sname);
            if (!site)
                site = ss->getDock(snloc);
        }

        if (site && site->updated > timestamp)
            continue;
        if (!site)
            site = ss->addEntity(tn, sname, snloc, {.ts=ge->timestamp});
        else
            site = ss->checkTypeAndName(site, tn, sname, snloc, {.ts=ge->timestamp});
        } catch (...) {
            LOG_ERROR("Uncatched exception in events: FSSSignalDiscovered");
        }
    }
    ss->save();
}

void updateBodyFromSpace(StarSystem* ss, spGameEvent& ge) {
    if (ss || !ss->systemAddress)
        return;
    if (!st::space.bodyId.has_value())
        return;
    if (!(st::space.bodyType == "Star" || st::space.bodyType == "Planet"))
        return;
    auto body = ss->getBodyById(st::space.bodyId);

    if (body) {
        TypeNav tp = toTypeNav(JsNavType::get(st::space.bodyType));
        if (body->type != tp) {
            LOG_ERROR("Had bad type '{}' must be '{}' for body '{}'", enum_name(body->type), tp, body->name);
            body = ss->checkTypeAndName(body, tp, st::space.bodyName, "", {.ts=ge->timestamp});
        }
        if (!body->nameEq(st::space.bodyName)) {
            LOG_WARNING("Had bad body name '{}' must be '{}'", body->name, st::space.bodyName);
            body = ss->checkTypeAndName(body, tp, st::space.bodyName, "", {.ts=ge->timestamp});
        }
    }
    else if (st::space.bodyType == "Star") {
        body = ss->addEntity(TypeNav::Star, st::space.bodyName, "", {.ts=ge->timestamp});
        body->bodyId = st::space.bodyId;
        body->updated = ge->timestamp;
    }
    else if (st::space.bodyType == "Planet") {
        body = ss->addEntity(TypeNav::Planet, st::space.bodyName, "", {.ts=ge->timestamp});
        body->bodyId = st::space.bodyId;
        body->updated = ge->timestamp;
    }

    ss->save();
}

std::unordered_multimap<std::string,UniqueBody> allUniqueBodies;

void updateFromDestination(StarSystem* ss, spGameEvent& ge) {
    if (!ss || !ss->systemAddress || ss->systemAddress != st::destination.systemAddress)
        return;

    auto entities = ss->getEntitiesSnapshot();

    auto& dname = st::destination.name;
    for (auto &b: entities) {
        if (b && b->nameEq(dname) && isBody(b->type))
            return; // destination is a known body
        if (b && !b->name.empty() && b->name[0] == '\t' && b->bodyId == st::destination.bodyId) {
            b = ss->checkTypeAndName(b, b->type, dname, "", {.ts=ge->timestamp});
            ss->needBlobSave = true;
            ss->save();
        }
    }
    std::string xx_name;
    std::string ru_name;
    NavType::localizeName(TypeNav::Other, dname, xx_name, ru_name);
    for (auto& e : entities) {
        if (!e)
            continue;
        if (e->nameEq(dname) || e->nameEq(xx_name) || e->nameEq(ru_name)) {
            switch (e->type) {
            case TypeNav::FleetCarrier:
            case TypeNav::SquadronCarrier:
                // destination contains full name of the targeted carrier,
                // so s->name==xx_name and s->nloc==ru_name
                if (e->getLocalisedName() != dname) {
                    e = ss->checkTypeAndName(e, e->type, xx_name, ru_name, {.ts=ge->timestamp});
                }
                // fall through
            case TypeNav::NavBeacon:
            case TypeNav::TouristBeacon:
            case TypeNav::SpaceInstallation:
            case TypeNav::ColonisationShip:
            case TypeNav::AnonMegaship:
            case TypeNav::Megaship:
            case TypeNav::PlanetaryThing:
            case TypeNav::PlanetaryPort:
            case TypeNav::PlanetaryInstallation:
            case TypeNav::EngineerPort:
            case TypeNav::Settlement:
            case TypeNav::SpaceConstrDepot:
            case TypeNav::PlanetaryConstrDepot:
                if (e->parentBodyId != st::destination.bodyId) {
                    e->parentBodyId = st::destination.bodyId;
                    ss->needBlobSave = true;
                }
                break;
            case TypeNav::Orbis:
            case TypeNav::Ocellus:
            case TypeNav::Dodec:
            case TypeNav::Coriolis:
            case TypeNav::AsteroidBase:
            case TypeNav::SpaceOutpost:
                if (e->bodyId != st::destination.bodyId) {
                    e->bodyId = st::destination.bodyId;
                    ss->needBlobSave = true;
                }
                break;
            }
            ss->save();
            return;
        }
    }

    for (auto[it, rangeEnd] = allUniqueBodies.equal_range(dname); it != rangeEnd; ++it) {
        assert (dname == it->first);
        const UniqueBody& ub = it->second;
        if (ub.type == TypeNav::Star || ub.type == TypeNav::Planet) {
            if (ub.bodyId == st::destination.bodyId.val && ub.systemName == ss->systemName) {
                ss->addEntity(ub.type, dname, "", {.body_id=st::destination.bodyId, .ts=ge->timestamp});
                ss->save();
                return;
            }
        }
    }

    if (dname == ss->systemName) {
        // not in unique stars list, so it's a main star name
        spEntity star = ss->addEntity(TypeNav::Star, dname, "", {.body_id=st::destination.bodyId, .ts=ge->timestamp});
    }
    else if (dname.size() > ss->systemName.size() && dname.starts_with(ss->systemName)) {
        auto suffix = dname.substr(ss->systemName.size());
        TypeNav tn = NavType::guessTypeFromName(TypeNav::Body, dname);
        if (tn == TypeNav::Body && suffix.size() >= 2 && suffix[0] == ' ') {
            auto spl = split(suffix.substr(1), ' ');
            if (spl.size() == 1) {
                auto& nm = spl[0];
                if (nm.size() >= 1 && nm[0] >= 'A' && nm[0] <= 'Z')
                    tn = TypeNav::Star;
                else if (nm.size() >= 1 && nm[0] >= '0' && nm[0] <= '9')
                    tn = TypeNav::Planet;
            }
            else if (spl.size() > 1) {
                auto& nm0 = spl[0];
                auto& nm1 = spl[1];
                if (nm0[0] >= 'A' && nm0[0] <= 'Z' && nm1[0] >= '0' && nm1[0] <= '9')
                    tn = TypeNav::Planet;
            }
        }
        ss->addEntity(tn, dname, "", {.body_id=st::destination.bodyId, .ts=ge->timestamp});
    }
    else {
        TypeNav tn = NavType::guessTypeFromName(TypeNav::Other, dname);
        spEntity site = ss->addEntity(tn, dname, "", {.ts=ge->timestamp});
        site->updated = ge->timestamp;
        if (isBody(site->type) || isSpaceStation(site->type))
            site->bodyId = st::destination.bodyId;
    }
    ss->save();
}


bool updateFromGameEvent(StarSystem* ss, spGameEvent& ge) {
    if (!ss || ge->expired || ge->timestamp < ss->updated_at)
        return false;

    auto& je = ge->data;

    struct StarSystem::Extra ge_ext {};
    auto& ss_ext = ss->ext;
    if (ge->event == "Location" || ge->event == "FSDJump" || ge->event == "CarrierJump") {
        ss->updated_at = ge->timestamp;
        ss->needCoreSave = true;

        ge_ext.allegiance = JsAllegiance::get_or(je["SystemAllegiance"].as_string_or(), badEnum);
        ge_ext.government = JsGovernment::get_or(je["SystemGovernment"].as_string_or(), badEnum);
        ge_ext.primaryEconomy = JsEconomy::get_or(je["SystemEconomy"].as_string_or(), badEnum);
        ge_ext.secondaryEconomy = JsEconomy::get_or(je["SystemSecondEconomy"].as_string_or(), badEnum);
        ge_ext.security = JsSecurity::get_or(je["SystemSecurity"].as_string_or(), badEnum);
        ge_ext.population = je["Population"].as_int_or(0);

        MERGE(ss_ext, ge_ext, allegiance);
        MERGE(ss_ext, ge_ext, government);
        MERGE(ss_ext, ge_ext, primaryEconomy);
        MERGE(ss_ext, ge_ext, secondaryEconomy);
        MERGE(ss_ext, ge_ext, security);
        MERGE(ss_ext, ge_ext, population);
        MERGE(ss_ext, ge_ext, allegiance);
        MERGE(ss_ext, ge_ext, allegiance);
    }

    if (ge->event == "NavBeaconScan") {
        ge_ext.bodyCount = je["NumBodies"].as_int_or(0);
        MERGE(ss_ext, ge_ext, bodyCount);
    }
    if (ge->event == "FSSDiscoveryScan") {
        ge_ext.bodyCount = je["BodyCount"].as_int_or(0);
        MERGE(ss_ext, ge_ext, bodyCount);
    }
    if (ge->event == "FSSAllBodiesFound") {
        ge_ext.bodyCount = je["Count"].as_int_or(0);
        MERGE(ss_ext, ge_ext, bodyCount);
    }

    ss->save();
    return true;
}

bool updateFromScanEvent(StarSystem* ss, spGameEvent& ge) {
    if (!ss || ge->expired || ge->timestamp < ss->updated_at)
        return false;

    auto& je = ge->data;

    ss->updated_at = ge->timestamp;
    ss->needCoreSave = true;

    spEntity body;

    if (ge->event == "ScanBaryCentre") {
        body_id_t bodyId(je["BodyID"].as_int_or(-1));
        body = ss->getBodyById(bodyId);
        if (!body) {
            body = ss->addEntity(TypeNav::Barycenter, "", "", {.body_id=bodyId, .ts=ge->timestamp});
            body->bodyId = bodyId;
            body->updated = ge->timestamp;
        }
        else if (body->type != TypeNav::Barycenter) {
            LOG_ERROR("Body of type {} appears to be Barycenter", enum_name(body->type));
            body = ss->checkTypeAndName(body, TypeNav::Barycenter, "", "", {.body_id=bodyId, .ts=ge->timestamp});
        }
        if (!body)
            return false;
    }

    if (ge->event == "Scan") {
        body_id_t bodyId(je["BodyID"].as_int_or(-1));
        body = ss->getBodyById(bodyId);

        if (je["StarType"].is_string()) {
            if (!body) {
                body = ss->addEntity(TypeNav::Star, je["BodyName"].as_string_or(), "",
                                     {.body_id=bodyId, .ts=ge->timestamp});
                body->bodyId = bodyId;
            }
            else if (body->type != TypeNav::Star) {
                LOG_ERROR("Entity of type {} appears to be Star", enum_name(body->type));
                body = ss->checkTypeAndName(body, TypeNav::Star, je["BodyName"].as_string_or(), "",
                                            {.body_id=bodyId, .ts=ge->timestamp});
            }
            if (!body)
                return false;

            struct StarData ge_ext {};
            if (je.has_key("DistanceFromArrivalLS") && je["DistanceFromArrivalLS"].as_real() == 0.0)
                ge_ext.mainStar = true;
            ge_ext.subType =            JsBodySubType::get_or(je["StarType"].as_string_or(), badEnum);
            ge_ext.subClass =           je["Subclass"].as_int_or(-1);
            ge_ext.luminosity =         JsLuminosity::get_or(je["Luminosity"].as_string_or(), badEnum);
            ge_ext.age =                je["Age_MY"].as_int_or();
            ge_ext.solarMasses =        je["StellarMass"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.absoluteMagnitude =  je["AbsoluteMagnitude"].as_real_or(std::numeric_limits<float>::quiet_NaN());

            auto* star_ext = body->getStarData();
            assert (star_ext);
            if (!star_ext)
                return false;
            //MERGE(star_ext, ge_ext, subClass);
            if (ge_ext.subClass >= 0 && star_ext->subClass != ge_ext.subClass)
                star_ext->subClass = ge_ext.subClass;
            MERGE(*star_ext, ge_ext, luminosity);
            MERGE(*star_ext, ge_ext, age);
            MERGE(*star_ext, ge_ext, solarMasses);
            MERGE(*star_ext, ge_ext, absoluteMagnitude);
        }
        else if (je["PlanetClass"].is_string()) {
            if (!body) {
                body = ss->addEntity(TypeNav::Planet, je["BodyName"].as_string_or(), "",
                                     {.body_id=bodyId, .ts=ge->timestamp});
                body->bodyId = bodyId;
            }
            if (body->type != TypeNav::Planet) {
                LOG_ERROR("Entity of type {} appears to be Planet", enum_name(body->type));
                body = ss->checkTypeAndName(body, TypeNav::Star, je["BodyName"].as_string_or(), "",
                                            {.body_id=bodyId, .ts=ge->timestamp});
            }
            if (!body)
                return false;

            struct PlanetData ge_ext {};
            if (je.has_key("Landable"))
                ge_ext.isLandable = je["Landable"].as_bool();
            ge_ext.subType =            JsBodySubType::get_or(je["PlanetClass"].as_string_or(), badEnum);
            ge_ext.volcanismType =      JsVolcanismType::get_or(je["Volcanism"].as_string_or(), badEnum);
            ge_ext.atmosphereType =     JsAtmosphereType::get_or(je["AtmosphereType"].as_string_or(), badEnum);
            ge_ext.terraformingState =  JsTerraformingState::get_or(je["TerraformState"].as_string_or(), badEnum);
            ge_ext.reserveLevel =       JsReserveLevel::get_or(je["ReserveLevel"].as_string_or(), badEnum);
            ge_ext.earthMasses =        je["MassEM"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.surfaceGravity =     je["SurfaceGravity"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.surfacePressure =    je["SurfacePressure"].as_real_or(std::numeric_limits<float>::quiet_NaN());

            auto* pl_ext = body->getPlanetData();
            assert (pl_ext);
            if (!pl_ext)
                return false;
            MERGE(*pl_ext, ge_ext, volcanismType);
            MERGE(*pl_ext, ge_ext, atmosphereType);
            MERGE(*pl_ext, ge_ext, terraformingState);
            MERGE(*pl_ext, ge_ext, reserveLevel);
            MERGE(*pl_ext, ge_ext, earthMasses);
            MERGE(*pl_ext, ge_ext, surfaceGravity);
            MERGE(*pl_ext, ge_ext, surfacePressure);
        }
        else if (gal::BELT.match_name(je["BodyName"].as_string_or())) {
            if (!body) {
                body = ss->addEntity(TypeNav::AsteroidCluster, je["BodyName"].as_string_or(), "",
                                     {.body_id=bodyId, .ts=ge->timestamp});
                body->bodyId = bodyId;
            }
            else if (body->type != TypeNav::AsteroidCluster) {
                LOG_ERROR("Entity of type {} appears to be AsteroidCluster", enum_name(body->type));
                body = ss->checkTypeAndName(body, TypeNav::AsteroidCluster, je["BodyName"].as_string_or(), "", {.ts=ge->timestamp});
            }
        }
        else {
            if (!body) {
                body = ss->addEntity(TypeNav::Body, je["BodyName"].as_string_or(), "",
                                     {.body_id=bodyId, .ts=ge->timestamp});
                body->bodyId = bodyId;
            }
            else if (!isBody(body->type)) {
                LOG_ERROR("Entity of type {} appears to be a body", enum_name(body->type));
                body = ss->checkTypeAndName(body, TypeNav::Body, je["BodyName"].as_string_or(), "",
                                            {.body_id=bodyId, .ts=ge->timestamp});
            }
        }

        body->updated = ge->timestamp;
        ss->needBlobSave = true;

        if (auto nm = je["BodyName"].as_string_or(); !nm.empty() && nm != body->name) {
            body = ss->checkTypeAndName(body, body->type, nm, "", {.body_id=bodyId, .ts=ge->timestamp});
        }

        if (je["Parents"].is_array()) {
            auto b = body;
            for (auto jp : je["Parents"].as_array()) {
                auto& map = jp.as_object_or();
                if (map.size() != 1)
                    break;
                std::string_view key = map.begin()->first;
                TypeNav p_type = toTypeNav(JsNavType::get_or(key, badEnum));
                body_id_t p_id;
                p_id = map.begin()->second.as_int_or(-1);
                if (!isBody(p_type) || !p_id.has_value())
                    break;
                if (b && b->parentBodyId != p_id) {
                    b->parentBodyId = p_id;
                    ss->needBlobSave = true;
                }
                auto p = ss->getBodyById(p_id);
                //if (!p) {
                //    std::string name;
                //    if (p_type == TypeNav::Star)
                //        name = std::format("\tStar#{}", p_id.val);
                //    else if (p_type == TypeNav::Planet)
                //        name = std::format("\tPlanet#{}", p_id.val);
                //    p = ss->addEntity(p_type, name, "", {.body_id=p_id, .ts=ge->timestamp});
                //    p->bodyId = p_id;
                //}
                if (!p && (p_type == TypeNav::Barycenter || p_type == TypeNav::Ring))
                    p = ss->addEntity(p_type, "", "", {.body_id=p_id, .ts=ge->timestamp});
                b = p;
            }
        }

        {
            struct BodyData ge_ext {};
            if (je.has_key("TidalLock")) {
                ge_ext.tidalLock = je["TidalLock"].as_bool();
            }
            ge_ext.radius =               je["Radius"].as_real_or(std::numeric_limits<float>::quiet_NaN()) / 1000.f; // meters->kilometers
            ge_ext.distanceToArrival =    je["DistanceFromArrivalLS"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.orbitalPeriod =        je["OrbitalPeriod"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.rotationalPeriod =     je["RotationPeriod"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.semiMajorAxis =        je["SemiMajorAxis"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.orbitalEccentricity =  je["Eccentricity"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.orbitalInclination =   je["OrbitalInclination"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.argOfPeriapsis =       je["Periapsis"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.meanAnomaly =          je["MeanAnomaly"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.ascendingNode =        je["AscendingNode"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.surfaceTemperature =   je["SurfaceTemperature"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.axialTilt =            je["AxialTilt"].as_real_or(std::numeric_limits<float>::quiet_NaN());

            auto* body_ext = body->getBodyData();
            assert (body_ext);
            if (!body_ext)
                return false;
            MERGE(*body_ext, ge_ext, tidalLock);
            MERGE(*body_ext, ge_ext, radius);
            MERGE(*body_ext, ge_ext, distanceToArrival);
            MERGE(*body_ext, ge_ext, rotationalPeriod);
            MERGE(*body_ext, ge_ext, orbitalPeriod);
            MERGE(*body_ext, ge_ext, semiMajorAxis);
            MERGE(*body_ext, ge_ext, orbitalEccentricity);
            MERGE(*body_ext, ge_ext, orbitalInclination);
            MERGE(*body_ext, ge_ext, argOfPeriapsis);
            MERGE(*body_ext, ge_ext, meanAnomaly);
            MERGE(*body_ext, ge_ext, ascendingNode);
            MERGE(*body_ext, ge_ext, surfaceTemperature);
            MERGE(*body_ext, ge_ext, axialTilt);
        }
    }

    ss->save();
    return true;
}

bool updateFromDockEvent(StarSystem* ss, spGameEvent& ge) {
    auto& je = ge->data;

    if (!ss || !ss->systemAddress)
        return false;

    if (ge->event=="SupercruiseDestinationDrop") {
        int64_t marketId = je["MarketID"].as_int_or();
        if (!marketId)
            return false;
        auto name = je["Type"].as_string_or();
        auto nloc = je["Type_Localised"].as_string_or();
        gal::spEntity dock = ss->getDock(marketId);
        if (!dock)
            dock = ss->getDock(name);
        if (!dock) {
            dock = ss->addEntity(TypeNav::AnySite, name, nloc, {.market_id=marketId, .ts=ge->timestamp});
        } else {
            dock = ss->checkTypeAndName(dock, dock->type, name, "", {.market_id=marketId, .ts=ge->timestamp});
        }
        if (!dock->marketId()) {
            dock->setMarketId(marketId);
            dock->updated = ge->timestamp;
            ss->needBlobSave = true;
        }
        ss->save();
        return true;
    }


    if (!(ge->event=="ApproachSettlement" || ge->event=="Docked" || ge->event=="DockingRequested" || (ge->event=="Location") && ge->data["Docked"]))
        return false;

    body_id_t stationBodyId;
    if (ge->event == "Location") {
        body_id_t bodyId;
        auto bodyName = je["Body"].as_string_or();
        auto bodyType = je["BodyType"].as_string_or();
        bodyId = je["BodyID"].as_int_or(-1);
        if (bodyType == "Star" || bodyType == "Planet") {
            TypeNav tp = toTypeNav(JsNavType::get(bodyType));
            auto body = ss->getBodyById(bodyId);
            if (!body)
                body = ss->getBody(bodyName);
            if (!body)
                body = ss->addEntity(tp, bodyName, "", {.body_id=bodyId, .ts=ge->timestamp});
            body = ss->checkTypeAndName(body, tp, bodyName, "", {.body_id=bodyId, .ts=ge->timestamp});
            if (body->bodyId != bodyId) {
                body->bodyId = bodyId;
                body->updated = ge->timestamp;
                ss->needBlobSave = true;
            }
        }
        else if (bodyType == "Station") {
            stationBodyId = bodyId;
        }
    }

    std::string sname;
    std::string snloc;
    std::string stype;
    int64_t marketId = je["MarketID"].as_int_or();
    if (ge->event=="ApproachSettlement") {
        sname = je["Name"].as_string_or();
        snloc = je["Name_Localised"].as_string_or();
        if (st::client.majorVersion <= 3) {
            while (sname.back() == '+') sname = sname.substr(0,sname.size()-1);
            while (sname.back() == ' ') sname = sname.substr(0,sname.size()-1);
        }
    } else {
        sname = je["StationName"].as_string_or();
        snloc = je["StationName_Localised"].as_string_or();
        stype = je["StationType"].as_string_or();
    }

    TypeNav typeNav = toTypeNav(JsNavType::get(stype));
    typeNav = NavType::guessTypeFromName(typeNav, sname);

    spEntity dock = ss->getDock(marketId);
    if (!dock) {
        dock = ss->getDock(sname);
        if (!dock && !snloc.empty())
            dock = ss->getDock(snloc);
        if (!dock)
            dock = ss->getSignal(sname);
        if (dock && !isAnonMegaship(dock->type)) {
            if (dock->marketId()) {
                assert (dock->marketId() != marketId);
                dock.reset();
            } else {
                dock->setMarketId(marketId);
                dock->updated = ge->timestamp;
                ss->needBlobSave = true;
            }
        }
    }

    if (typeNav == TypeNav::Other && marketId)
        typeNav = TypeNav::AnySite;
    if (isSignal(typeNav) || isBody(typeNav)) // like "ApproachSettlement" for TouristBeacon
        return false;

    if (!dock && isMegaship(typeNav)) {
        db::MegashipJS db_ms {};
        db_ms.marketId = marketId;
        db_ms.name = sname;
        if (db::loadMegaShip(db_ms, true) && db_ms.marketId) {
            if (db_ms.systemId != ss->systemAddress)
                dock = ss->moveMegashipHere(db_ms.marketId, db_ms.code, db_ms.name, body_id_t(), false);
        }
    }
    if (typeNav == TypeNav::PlanetaryPort || typeNav == TypeNav::PlanetaryThing) {
        if (je["StationGovernment"].as_string_or() == "$government_Engineer;")
            typeNav = TypeNav::EngineerPort;
    }
    if (!dock)
        dock = ss->addEntity(typeNav, sname, snloc, {.market_id=marketId, .ts=ge->timestamp});
    else
        dock = ss->checkTypeAndName(dock, typeNav, sname, snloc, {.market_id=marketId, .ts=ge->timestamp});

    if (!dock->parentBodyId.has_value()) {
        if (st::space.bodyType == "Planet" || st::space.bodyType == "Star") {
            dock->parentBodyId = st::space.bodyId;
            dock->updated = ge->timestamp;
            ss->needBlobSave = true;
        }
    }
    if (marketId && !dock->marketId() && !isAnonMegaship(dock->type)) {
        dock->setMarketId(marketId);
        ss->needBlobSave = true;
    }

    struct StationData ge_ext {};
    ge_ext.marketId = je["MarketID"].as_int_or();
    if (je["StationFaction"].is_object())
        ge_ext.controllingFaction = je["StationFaction"]["Name"].as_string_or();
    ge_ext.allegiance = JsAllegiance::get_or(je["StationAllegiance"].as_string_or(), badEnum);
    ge_ext.government = JsGovernment::get_or(je["StationGovernment"].as_string_or(), badEnum);
    ge_ext.state = JsStationState::get_or(je["StationState"].as_string_or(), badEnum);
    ge_ext.controllingFactionState = JsFactionState::get_or(je["FactionState"].as_string_or(), badEnum);
    ge_ext.primaryEconomy = JsEconomy::get_or(je["StationEconomy"].as_string_or(), badEnum);
    if (auto& arr = je["StationEconomies"].as_array_or(); !arr.empty()) {
        ge_ext.economies.reserve(arr.size());
        for (auto &item: arr) {
            JsEconomy economy = JsEconomy::get_or(item["Name"].as_string(), badEnum);
            float prop = item["Proportion"].as_real();
            ge_ext.economies.emplace_back(economy, prop);
        }
    }
    //ge_ext.carrierDockingAccess = JsCarrierDockingAccess::get_or(je["StationState"].as_string_or(), badEnum);
    ge_ext.distanceToArrival =    je["DistFromStarLS"].as_real_or(std::numeric_limits<float>::quiet_NaN());
    ge_ext.latitude =             je["Latitude"].as_real_or(std::numeric_limits<float>::quiet_NaN());
    ge_ext.longitude =            je["Longitude"].as_real_or(std::numeric_limits<float>::quiet_NaN());
    if (je["LandingPads"].is_object()) {
        ge_ext.landingPads.large = je["LandingPads"]["Large"].as_int_or();
        ge_ext.landingPads.medium = je["LandingPads"]["Medium"].as_int_or();
        ge_ext.landingPads.small = je["LandingPads"]["Small"].as_int_or();
    }
    if (auto& arr = je["StationServices"].as_array_or(); !arr.empty()) {
        ge_ext.services.reserve(arr.size());
        for (auto &item: arr) {
            JsServices service = JsServices::get_or(item.as_string(), badEnum);
            ge_ext.services.push_back(service);
        }
    }

    auto* st_ext = dock->getStationData();
    assert (st_ext);
    if (!st_ext)
        return false;

    MERGE(*st_ext, ge_ext, marketId);
    MERGE(*st_ext, ge_ext, controllingFaction);
    MERGE(*st_ext, ge_ext, allegiance);
    MERGE(*st_ext, ge_ext, government);
    MERGE(*st_ext, ge_ext, state);
    MERGE(*st_ext, ge_ext, controllingFactionState);
    MERGE(*st_ext, ge_ext, primaryEconomy);
    MERGE(*st_ext, ge_ext, economies);
    MERGE(*st_ext, ge_ext, distanceToArrival);
    MERGE(*st_ext, ge_ext, latitude);
    MERGE(*st_ext, ge_ext, longitude);
    MERGE(*st_ext, ge_ext, landingPads);
    MERGE(*st_ext, ge_ext, services);

    return true;
}

bool updateFromMarketEvent(StarSystem* ss, spGameEvent& ge) {
    auto& je = ge->data;

    if (!ss)
        return false;

    if (ge->event == "ColonisationConstructionDepot") {
        int64_t marketId = je["MarketID"].as_int_or();
        spMarket old_market = gal::getMarket(marketId);
        if (old_market && old_market->timestamp >= ge->timestamp)
            return false;
        auto dock = ss->getDock(marketId);
        spMarket market = std::make_shared<Market>(Market{
                .timestamp = ge->timestamp,
                .marketId = marketId,
                .systemAddress = ss->systemAddress,
                .stationName = dock ? dock->name : "",
                .stationType = "ConstrDepot",
                .starSystem = ss->systemName,
        });
        if (old_market)
            market->raven = old_market->raven;
        bool complete = je["ConstructionComplete"].as_bool();
        bool hasRaven = Cfg.isRavenColonialEnabled() && !market->ravenBuildId().empty();
        bool reportToRaven = false;
        if (hasRaven) {
            if (!ge->expired && !market->raven->timestamp)
                reportToRaven = true;
            else if (complete && market->raven->status != "complete")
                reportToRaven = true;
            else {
                // check if there was a contribution past last raven project update
                for (auto &ci: market->raven->commanders) {
                    if (ci.second.timestamp > market->raven->timestamp)
                        reportToRaven = true;
                }
            }
        }
        for (auto &jr: je["ResourcesRequired"].as_array_or()) {
            if (!jr["Name"].is_string())
                continue;
            std::string name = *jr["Name"].as_string();
            // "$aluminium_name;"
            if (name.empty() || name[0] != '$' || !name.ends_with("_name;"))
                continue;
            name = name.substr(1, name.size() - 7);
            Commodity *commodity = Cfg.getCommodityById(name);
            if (!commodity)
                continue;
            int provided = jr["ProvidedAmount"].as_int_or();
            int required = jr["RequiredAmount"].as_int_or();
            if (hasRaven && old_market) {
                if (!old_market->items.contains(commodity))
                    reportToRaven = true;
                else {
                    auto &old_ml = old_market->items.at(commodity);
                    if (old_ml.stock != provided || old_ml.demand != required)
                        reportToRaven = true;
                }
            }
            MarketLine ml{};
            ml.sellPrice = jr["Payment"].as_int_or();
            ml.stock = provided;
            ml.demand = required;
            ml.isConsumer = true;
            ml.isProducer = false;
            market->items.emplace(commodity, ml);
        }

        if (hasRaven && reportToRaven) {
            if (market->ravenBuildId().empty() || market->raven->timestamp >= ge->timestamp)
                reportToRaven = false;
            else if (market->raven->status == "complete")
                reportToRaven = false;
            else
                market->raven->timestamp = ge->timestamp;
        }
        gal::setMarketData(market);
        if (hasRaven && reportToRaven) {
            RavenColonial::reportConstructionDepot(ge, market);
        } else if (complete) {
            if (!market->raven)
                market->raven = std::make_shared<RavenProj>();
            if (market->raven->status != "complete") {
                market->raven->status = "complete";
                gal::saveMarket(market.get());
            }
        }

        return true;
    }

    return false;
}

bool fill_star_system_for_db(const StarSystem* ss, db::StarSystemJS& db_ss, std::vector<db::MegashipJS>& db_megaships) {
    if (!ss || !ss->systemAddress)
        return false;
    db_ss.address = ss->systemAddress;
    db_ss.name = ss->systemName;
    db_ss.coords = {ss->systemCoords.x, ss->systemCoords.y, ss->systemCoords.z};
    db_ss.allegiance = ss->ext.allegiance;
    db_ss.government = ss->ext.government;
    db_ss.primaryEconomy = ss->ext.primaryEconomy;
    db_ss.secondaryEconomy = ss->ext.secondaryEconomy;
    db_ss.security = ss->ext.security;
    db_ss.population = ss->ext.population;
    db_ss.bodyCount = ss->ext.bodyCount;
    db_ss.updated_at = ss->eddn_updated_at;

    auto ss_entities = ss->getEntitiesSnapshot();
    std::stable_sort(ss_entities.begin(), ss_entities.end(), [](auto& e1, auto&e2)->bool {
        if (e1->bodyId.val == e2->bodyId.val)
            return e1->type < e2->type;
        return unsigned(e1->bodyId.val) < unsigned(e2->bodyId.val);
    });

    int count_bodies = 0;
    for (auto &body: ss_entities) {
        if (isBody(body->type) && body->bodyId.has_value())
            count_bodies += 1;
    }
    db_ss.bodies.reserve(count_bodies);
    for (auto &body: ss_entities) {
        if (!isBody(body->type) || !body->bodyId.has_value())
            continue;
        JsNavType bt = toJsNavType(body->type);
        if (!bt.has_value())
            continue;

        auto &b = db_ss.bodies.emplace_back();
        b.type = bt;
        b.bodyId = body->bodyId;
        b.name = body->name;
        b.updated_at = body->updated;

        const auto* body_ext = body->getBodyData();
        assert (body_ext);
        if (!body_ext)
            return false;
        {
            b.tidalLock = body_ext->tidalLock;
            b.radius = body_ext->radius;
            b.distanceToArrival = body_ext->distanceToArrival;
            b.rotationalPeriod = body_ext->rotationalPeriod;
            b.orbitalPeriod = body_ext->orbitalPeriod;
            b.semiMajorAxis = body_ext->semiMajorAxis;
            b.orbitalEccentricity = body_ext->orbitalEccentricity;
            b.orbitalInclination = body_ext->orbitalInclination;
            b.argOfPeriapsis = body_ext->argOfPeriapsis;
            b.meanAnomaly = body_ext->meanAnomaly;
            b.ascendingNode = body_ext->ascendingNode;
            b.surfaceTemperature = body_ext->surfaceTemperature;
            b.axialTilt = body_ext->axialTilt;
            if (body->parentBodyId.has_value()) {
                spEntity pb = ss->getBodyById(body->parentBodyId);
                while (pb && pb->bodyId.has_value()) {
                    JsNavType pt = toJsNavType(pb->type);
                    if (!pt.has_value() || !isBody(pb->type))
                        break;
                    b.parents.push_back({pt, pb->bodyId.val});
                    pb = ss->getBodyById(pb->parentBodyId);
                }
            }
        }
        if (body->type == TypeNav::Star) {
            const auto* star_ext = body->getStarData();
            assert (star_ext);
            if (!star_ext)
                return false;
            b.subType = star_ext->subType;
            b.ensureStarPart()->mainStar = star_ext->mainStar;
            b.ensureStarPart()->subClass = star_ext->subClass;
            b.ensureStarPart()->luminosity = star_ext->luminosity;
            b.ensureStarPart()->age = star_ext->age;
            b.ensureStarPart()->solarMasses = star_ext->solarMasses;
            b.ensureStarPart()->absoluteMagnitude = star_ext->absoluteMagnitude;
        }
        if (body->type == TypeNav::Planet) {
            const auto* pl_ext = body->getPlanetData();
            assert (pl_ext);
            if (!pl_ext)
                return false;
            b.subType = pl_ext->subType;
            b.ensurePlanedPart()->isLandable = pl_ext->isLandable;
            b.ensurePlanedPart()->volcanismType = pl_ext->volcanismType;
            b.ensurePlanedPart()->atmosphereType = pl_ext->atmosphereType;
            b.ensurePlanedPart()->terraformingState = pl_ext->terraformingState;
            b.ensurePlanedPart()->reserveLevel = pl_ext->reserveLevel;
            b.ensurePlanedPart()->earthMasses = pl_ext->earthMasses;
            b.ensurePlanedPart()->surfaceGravity = pl_ext->surfaceGravity;
            b.ensurePlanedPart()->surfacePressure = pl_ext->surfacePressure;
        }
    }
    int count_stations = 0;
    for (auto &station: ss_entities) {
        if (isDatabaseSite(station->type))
            count_stations += 1;
    }
    db_ss.stations.reserve(count_stations);
    for (auto &site: ss_entities) {
        if (!isDatabaseSite(site->type))
            continue;
        JsNavType st = toJsNavType(site->type);
        if (!st.has_value())
            continue;

        if (isMegaship(site->type)) {
            db::MegashipJS& db_ms = db_megaships.emplace_back();
            fill_megaship_for_db(ss, site.get(), db_ms);
            continue;
        }

        if (!isAnySite(site->type)) {
            db::SignalJS& sig = db_ss.signals.emplace_back();
            sig.type = st;
            sig.name = site->name;
            //sig.latitude = ext.latitude;
            //sig.longitude = ext.longitude;
            continue;
        }

        db::StationJS& s = db_ss.stations.emplace_back();
        s.marketId = site->marketId();
        s.type = st;
        if (site->bodyId.has_value()) s.bodyId = site->bodyId;
        if (site->parentBodyId.has_value()) s.parentId = site->parentBodyId;
        s.updated_at = site->updated;

        s.name = site->name;
        if (!site->getLocalisedName().empty() && isCarrier(site->type))
            s.name = site->getLocalisedName();

        const auto* ext = site->getStationData();
        assert (ext);
        if (!ext)
            return false;
        s.var = ext->unexpanded_name;
        s.controllingFaction = ext->controllingFaction;
        s.allegiance = ext->allegiance;
        s.government = ext->government;
        s.state = ext->state;
        s.controllingFactionState = ext->controllingFactionState;
        s.primaryEconomy = ext->primaryEconomy;
        s.economies = ext->economies;
        s.carrierDockingAccess = ext->carrierDockingAccess;
        s.distanceToArrival = ext->distanceToArrival;
        s.latitude = ext->latitude;
        s.longitude = ext->longitude;
        s.landingPads = ext->landingPads;
        s.services = ext->services;
    }

    return true;
}

bool fill_megaship_for_db(const StarSystem* ss, const Entity* megaship, db::MegashipJS& db_ms) {
    if (!ss || !ss->systemAddress || !megaship)
        return false;
    db_ms.systemId = ss->systemAddress;
    db_ms.updated = megaship->updated;
    if (isCarrier(megaship->type)) {
        db_ms.code = megaship->name;
        db_ms.name = megaship->getLocalisedName();
        if (db_ms.name.empty())
            db_ms.name = db_ms.code;
        db_ms.marketId = megaship->marketId();
    }
    else if (isAnonMegaship(megaship->type)) {
        db_ms.code = "";
        db_ms.anon = true;
        db_ms.name = megaship->name;
    }
    else if (megaship->name == "Stronghold Carrier" ||
            megaship->name == "Trailblazer Dream" ||
            megaship->name == "System Colonisation Ship")
    {
        db_ms.code = "";
        db_ms.anon = true;
        db_ms.name = megaship->name;
    }
    else {
        db_ms.code = "";
        db_ms.name = megaship->name;
        db_ms.marketId = megaship->marketId();
    }

    db_ms.blob.type = toJsNavType(megaship->type);
    if (megaship->parentBodyId.has_value())
        db_ms.blob.parentId = megaship->parentBodyId;

    const auto* ext = megaship->getStationData();
    assert (ext);
    if (!ext)
        return false;
    db_ms.blob.var = ext->unexpanded_name;
    db_ms.blob.controllingFaction = ext->controllingFaction;
    db_ms.blob.allegiance = ext->allegiance;
    db_ms.blob.government = ext->government;
    db_ms.blob.state = ext->state;
    db_ms.blob.controllingFactionState = ext->controllingFactionState;
    db_ms.blob.primaryEconomy = ext->primaryEconomy;
    db_ms.blob.economies = ext->economies;
    db_ms.blob.carrierDockingAccess = ext->carrierDockingAccess;
    db_ms.blob.distanceToArrival = ext->distanceToArrival;
    db_ms.blob.landingPads = ext->landingPads;
    db_ms.blob.services = ext->services;
    return true;
}


} // namespace gal

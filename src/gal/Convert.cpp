//
// Created by mkizub on 21.09.2026.
//

#include "Galaxy.h"
#include "../db/DB.h"

namespace gal {

static void update_body_id_and_parents(gal::StarSystem* ss, gal::spEntity &entity, db::BodyJS& db_body) {
    if (db_body.bodyId >= 0 && entity->bodyId < 0) {
        entity->bodyId = db_body.bodyId;
        ss->needBlobSave = true;
    }

    if (!db_body.parents.empty()) {
        entity->parentBodyId = db_body.parents[0].bodyId;
        auto b = entity;
        for (const auto &jp: db_body.parents) {
            TypeNav p_type = toTypeNav(jp.type);
            int p_id = jp.bodyId;
            if (!isBody(p_type))
                break;
            if (b->parentBodyId != p_id) {
                b->parentBodyId = p_id;
                ss->needBlobSave = true;
            }
            auto p = ss->getBodyById(p_id);
            if (!p) {
                p = std::make_shared<gal::Entity>();
                p->setType(p_type);
                p->bodyId = p_id;
                ss->addEntity(p);
                ss->needBlobSave = true;
            }
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

bool update_station(StarSystem* ss, db::StationJS& db_st) {
    TypeNav tp = TypeNav::SpaceStation;
    if (db_st.latitude.has_value() || db_st.longitude.has_value())
        tp = TypeNav::PlanetaryThing;
    if (db_st.type.has_value())
        tp = toTypeNav(db_st.type);

    if (tp == TypeNav::PlanetaryInstallation && db_st.name == "Stronghold Carrier")
        tp = TypeNav::StrongholdCarrier;
    if (tp == TypeNav::PlanetaryThing && db_st.latitude.has_value())
        tp = TypeNav::PlanetaryInstallation;

    if (!(isSpaceSite(tp) || isPlanetarySite(tp)))
        return false;

    spEntity st;

    if (db_st.marketId > 0)
        st = ss->getDock(db_st.marketId);
    if (!st && !db_st.name.empty())
        st = ss->getDock(db_st.name);
    if (!st && db_st.bodyId > 0)
        st = ss->getBodyById(db_st.bodyId);

    if (!st) {
        st.reset(new Entity);
        st->bodyId = db_st.bodyId;
        st->setType(tp);
        st->setName(db_st.name);
        ss->addEntity(st);
        ss->needBlobSave = true;
    } else if (st->type != tp) {
        if (isSignal(st->type) || st->type == TypeNav::SpaceStation || st->type == TypeNav::PlanetaryThing) {
            st->setType(tp);
            ss->needBlobSave = true;
        }
    }
    if (st->name.empty()) {
        st->setName(db_st.name);
        ss->needBlobSave = true;
    }
    if (st->bodyId <= 0 && db_st.bodyId > 0) {
        st->bodyId = db_st.bodyId;
        ss->needBlobSave = true;
    }

    {
        auto &ext = st->getStationData();

        MERGE(ext, db_st, controllingFaction);
        MERGE(ext, db_st, allegiance);
        MERGE(ext, db_st, government);
        MERGE(ext, db_st, state);
        MERGE(ext, db_st, controllingFactionState);
        MERGE(ext, db_st, primaryEconomy);
        MERGE(ext, db_st, secondaryEconomy);
        MERGE(ext, db_st, carrierDockingAccess);
        MERGE(ext, db_st, economies);
        MERGE(ext, db_st, distanceToArrival);
        MERGE(ext, db_st, latitude);
        MERGE(ext, db_st, longitude);
        MERGE(ext, db_st, landingPads);
        MERGE(ext, db_st, services);
    }

    return true;
}

bool update_body(StarSystem* ss, db::BodyJS& db_body) {
    if (db_body.bodyId < 0)
        return false;

    TypeNav tp = toTypeNav(db_body.type);
    if (!isBody(tp))
        return false;

    spEntity body;

    body = ss->getBodyById(db_body.bodyId);
    if (!body) {
        body.reset(new Entity);
        body->bodyId = db_body.bodyId;
        body->setType(tp);
        body->setName(db_body.name);
        ss->addEntity(body);
        ss->needBlobSave = true;
    } else if (body->type != tp) {
        if (isSignal(body->type) || body->type == TypeNav::Body) {
            body->setType(tp);
            ss->needBlobSave = true;
        }
    }
    if (body->name.empty()) {
        body->setName(db_body.name);
        ss->needBlobSave = true;
    }

    update_body_id_and_parents(ss, body, db_body);

    {
        auto &ext = body->getBodyData();
        MERGE(ext, db_body, subType);
        MERGE(ext, db_body, tidalLock);
        MERGE(ext, db_body, radius);
        MERGE(ext, db_body, distanceToArrival);
        MERGE(ext, db_body, rotationalPeriod);
        MERGE(ext, db_body, orbitalPeriod);
        MERGE(ext, db_body, semiMajorAxis);
        MERGE(ext, db_body, orbitalEccentricity);
        MERGE(ext, db_body, orbitalInclination);
        MERGE(ext, db_body, argOfPeriapsis);
        MERGE(ext, db_body, meanAnomaly);
        MERGE(ext, db_body, ascendingNode);
        MERGE(ext, db_body, surfaceTemperature);
        MERGE(ext, db_body, axialTilt);
    }
    if (body->type == TypeNav::Star && db_body.getStarPart()) {
        auto &ext = body->getStarData();
        auto* sp = db_body.getStarPart();
        MERGE(ext, *sp, mainStar);
        MERGE(ext, *sp, spectralClass);
        MERGE(ext, *sp, luminosity);
        MERGE(ext, *sp, age);
        MERGE(ext, *sp, solarMasses);
        MERGE(ext, *sp, absoluteMagnitude);
    }
    if (body->type == TypeNav::Planet && db_body.getPlanetPart()) {
        auto &ext = body->getPlanetData();
        auto* sp = db_body.getPlanetPart();
        MERGE(ext, *sp, isLandable);
        MERGE(ext, *sp, volcanismType);
        MERGE(ext, *sp, atmosphereType);
        MERGE(ext, *sp, terraformingState);
        MERGE(ext, *sp, reserveLevel);
        MERGE(ext, *sp, earthMasses);
        MERGE(ext, *sp, surfaceGravity);
        MERGE(ext, *sp, surfacePressure);
    }

    for (auto& db_st : db_body.stations) {
        update_station(ss, db_st);
    }

    return true;
}

bool update_star_system(StarSystem* ss, db::StarSystemJS& db_ss) {
    if (!ss)
        return false;

    cv::Point3d systemPos{db_ss.coords.x, db_ss.coords.y, db_ss.coords.z};
    if (cv::norm(ss->starPos - systemPos) > 0.1) {
        ss->starPos = systemPos;
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
        update_station(ss, db_st);
    }

    return true;
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

        ge_ext.allegiance = JsAllegiance::get(je["SystemAllegiance"].as_string_or());
        ge_ext.government = JsGovernment::get(je["SystemGovernment"].as_string_or());
        ge_ext.primaryEconomy = JsEconomy::get(je["SystemEconomy"].as_string_or());
        ge_ext.secondaryEconomy = JsEconomy::get(je["SystemSecondEconomy"].as_string_or());
        ge_ext.security = JsSecurity::get(je["SystemSecurity"].as_string_or());
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

    if (ge->event == "ScanBaryCentre") {
        int bodyId = je["BodyID"].as_int();
        auto body = ss->getBodyById(bodyId);
        if (!body) {
            body = std::make_shared<gal::Entity>();
            body->setType(TypeNav::Barycenter);
            body->bodyId = bodyId;
            ss->addEntity(body);
        }
        else if (body->type != TypeNav::Barycenter) {
            body->setType(TypeNav::Barycenter);
            ss->needBlobSave = true;
        }
    }

    if (ge->event == "Scan") {
        struct Entity::PlanetData planet_ext {};

        int bodyId = je["BodyID"].as_int_or(-1);
        auto body = ss->getBodyById(bodyId);
        if (!body) {
            body = std::make_shared<gal::Entity>();
            body->bodyId = bodyId;
            ss->addEntity(body);
            ss->needBlobSave = true;
        }
        if (auto nm = je["BodyName"].as_string_or(); !nm.empty() && nm != body->name) {
            body->setName(nm);
            ss->needBlobSave = true;
        }

        body->updated = ge->timestamp;
        ss->needBlobSave = true;

        if (je["StarType"].is_string()) {
            if (body->type != TypeNav::Star) {
                body->setType(TypeNav::Star);
                ss->needBlobSave = true;
            }
            std::string code = *je["StarType"].as_string();
            if (je["Subclass"].is_int())
                code += std::to_string(je["Subclass"].as_int());
            if (body->code != code) {
                body->code = code;
                ss->needBlobSave = true;
            }

            struct Entity::StarData ge_ext {};
            auto& star_ext = body->getStarData();
            //opt_bool mainStar; // TODO: how to detect main star?
            if (je.has_key("DistanceFromArrivalLS") && je["DistanceFromArrivalLS"].as_real() == 0.0)
                ge_ext.mainStar = true;
            ge_ext.spectralClass =        JsSpectralClass::get(code);
            ge_ext.luminosity =           JsLuminosity::get(je["Luminosity"].as_string_or());
            ge_ext.age =                  je["Age_MY"].as_int_or();
            ge_ext.solarMasses =          je["StellarMass"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.absoluteMagnitude =    je["AbsoluteMagnitude"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            MERGE(star_ext, ge_ext, spectralClass);
            MERGE(star_ext, ge_ext, luminosity);
            MERGE(star_ext, ge_ext, age);
            MERGE(star_ext, ge_ext, solarMasses);
            MERGE(star_ext, ge_ext, absoluteMagnitude);
        }
        else if (je["PlanetClass"].is_string()) {
            if (body->type != TypeNav::Planet) {
                body->setType(TypeNav::Planet);
                ss->needBlobSave = true;
            }
            bool landable = (bool) je["Landable"];
            if (landable != body->special) {
                body->special = landable;
                ss->needBlobSave = true;
            }
            if (je.has_key("Landable"))
                planet_ext.isLandable = je["Landable"].as_bool();

            struct Entity::PlanetData ge_ext {};
            auto& pl_ext = body->getPlanetData();
            ge_ext.volcanismType =      JsVolcanismType::get(je["Volcanism"].as_string_or());
            ge_ext.atmosphereType =     JsAtmosphereType::get(je["AtmosphereType"].as_string_or());
            ge_ext.terraformingState =  JsTerraformingState::get(je["TerraformState"].as_string_or());
            ge_ext.reserveLevel =       JsReserveLevel::get(je["ReserveLevel"].as_string_or());
            ge_ext.earthMasses =        je["MassEM"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.surfaceGravity =     je["SurfaceGravity"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            ge_ext.surfacePressure =    je["SurfacePressure"].as_real_or(std::numeric_limits<float>::quiet_NaN());
            MERGE(pl_ext, ge_ext, volcanismType);
            MERGE(pl_ext, ge_ext, atmosphereType);
            MERGE(pl_ext, ge_ext, terraformingState);
            MERGE(pl_ext, ge_ext, reserveLevel);
            MERGE(pl_ext, ge_ext, earthMasses);
            MERGE(pl_ext, ge_ext, surfaceGravity);
            MERGE(pl_ext, ge_ext, surfacePressure);
        }
        else {
            if (gal::BELT.match_name(je["BodyName"].as_string_or())) {
                if (body->type != TypeNav::AsteroidCluster) {
                    body->setType(TypeNav::AsteroidCluster);
                    ss->needBlobSave = true;
                }
            } else if (!isBody(body->type)) {
                body->setType(TypeNav::Body);
                ss->needBlobSave = true;
            }
        }
        if (je["Parents"].is_array()) {
            auto b = body;
            for (auto jp : je["Parents"].as_array()) {
                auto& map = jp.as_object_or();
                if (map.size() != 1)
                    break;
                std::string_view key = map.begin()->first;
                TypeNav p_type = toTypeNav(JsBodyType::get(key));
                int p_id = map.begin()->second.as_int_or(-1);
                if (!isBody(p_type) || p_id < 0)
                    break;
                if (b->parentBodyId != p_id) {
                    b->parentBodyId = p_id;
                    ss->needBlobSave = true;
                }
                auto p = ss->getBodyById(p_id);
                if (!p) {
                    p = std::make_shared<gal::Entity>();
                    p->setType(p_type);
                    p->bodyId = p_id;
                    ss->addEntity(p);
                    ss->needBlobSave = true;
                }
                b = p;
            }
        }

        {
            struct Entity::BodyData ge_ext {};
            auto& body_ext = body->getBodyData();
            if (je.has_key("TidalLock")) {
                ge_ext.tidalLock = je["TidalLock"].as_bool();
                MERGE(body_ext, ge_ext, tidalLock);
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
            MERGE(body_ext, ge_ext, radius);
            MERGE(body_ext, ge_ext, distanceToArrival);
            MERGE(body_ext, ge_ext, rotationalPeriod);
            MERGE(body_ext, ge_ext, orbitalPeriod);
            MERGE(body_ext, ge_ext, semiMajorAxis);
            MERGE(body_ext, ge_ext, orbitalEccentricity);
            MERGE(body_ext, ge_ext, orbitalInclination);
            MERGE(body_ext, ge_ext, argOfPeriapsis);
            MERGE(body_ext, ge_ext, meanAnomaly);
            MERGE(body_ext, ge_ext, ascendingNode);
            MERGE(body_ext, ge_ext, surfaceTemperature);
            MERGE(body_ext, ge_ext, axialTilt);
        }
    }

    ss->save();
    return true;
}


bool fill_star_system_db(StarSystem* ss, db::StarSystemJS& db_ss) {
    db_ss.address = ss->systemAddress;
    db_ss.name = ss->systemName;
    db_ss.coords = {ss->starPos.x, ss->starPos.y, ss->starPos.z};
    db_ss.allegiance = ss->ext.allegiance;
    db_ss.government = ss->ext.government;
    db_ss.primaryEconomy = ss->ext.primaryEconomy;
    db_ss.secondaryEconomy = ss->ext.secondaryEconomy;
    db_ss.security = ss->ext.security;
    db_ss.population = ss->ext.population;
    db_ss.bodyCount = ss->ext.bodyCount;
    db_ss.updated_at = ss->eddn_updated_at;

    int count_bodies = 0;
    for (auto &body: ss->entities) {
        if (isBody(body->type) && body->bodyId >= 0)
            count_bodies += 1;
    }
    db_ss.bodies.reserve(count_bodies);
    for (auto &body: ss->entities) {
        if (!isBody(body->type) || body->bodyId < 0)
            continue;
        JsBodyType bt = toJsBodyType(body->type);
        if (!bt.has_value())
            continue;

        // Basically, we just have to make shure we don't store bodyId < 0 into DB
        //if (body->bodyId == 0) {
        //    if (body->type == TypeNav::Barycenter)
        //        continue;
        //    if (body->type != TypeNav::Star)
        //        continue;
        //    const auto &ext = body->getStarData();
        //    if (!ext.mainStar)
        //        continue;
        //}
        auto &b = db_ss.bodies.emplace_back();
        b.type = bt;
        b.bodyId = body->bodyId;
        b.name = body->name;
        b.updated_at = body->updated;

        {
            const auto &ext = body->getBodyData();
            b.subType = ext.subType;
            b.tidalLock = ext.tidalLock;
            b.orbitalPeriod = ext.orbitalPeriod;
            b.semiMajorAxis = ext.semiMajorAxis;
            b.orbitalEccentricity = ext.orbitalEccentricity;
            b.orbitalInclination = ext.orbitalInclination;
            b.argOfPeriapsis = ext.argOfPeriapsis;
            b.meanAnomaly = ext.meanAnomaly;
            b.ascendingNode = ext.ascendingNode;
            b.distanceToArrival = ext.distanceToArrival;
            b.surfaceTemperature = ext.surfaceTemperature;
            b.rotationalPeriod = ext.rotationalPeriod;
            b.axialTilt = ext.axialTilt;
            if (body->parentBodyId >= 0) {
                gal:spEntity pb = ss->getBodyById(body->parentBodyId);
                while (pb && pb->bodyId >= 0) {
                    JsBodyType pt = toJsBodyType(pb->type);
                    if (!pt.has_value())
                        break;
                    b.parents.push_back({pt, pb->bodyId});
                    pb = ss->getBodyById(pb->parentBodyId);
                }
            }
        }
        if (body->type == TypeNav::Star) {
            const auto &ext = body->getStarData();
            b.ensureStarPart()->mainStar = ext.mainStar;
            b.ensureStarPart()->spectralClass = ext.spectralClass;
            b.ensureStarPart()->luminosity = ext.luminosity;
            b.ensureStarPart()->age = ext.age;
            b.ensureStarPart()->solarMasses = ext.solarMasses;
            b.ensureStarPart()->absoluteMagnitude = ext.absoluteMagnitude;
        }
        if (body->type == TypeNav::Planet) {
            const auto &ext = body->getPlanetData();
            b.ensurePlanedPart()->isLandable = ext.isLandable;
            b.ensurePlanedPart()->volcanismType = ext.volcanismType;
            b.ensurePlanedPart()->atmosphereType = ext.atmosphereType;
            b.ensurePlanedPart()->terraformingState = ext.terraformingState;
            b.ensurePlanedPart()->reserveLevel = ext.reserveLevel;
            b.ensurePlanedPart()->earthMasses = ext.earthMasses;
            b.ensurePlanedPart()->surfaceGravity = ext.surfaceGravity;
            b.ensurePlanedPart()->surfacePressure = ext.surfacePressure;
        }
    }
    int count_stations = 0;
    for (auto &station: ss->entities) {
        if (station->type >= TypeNav::SpaceStation)
            count_stations += 1;
    }
    db_ss.stations.reserve(count_stations);
    for (auto &station: ss->entities) {
        JsStationType st = toJsStationType(station->type);
        if (!st.has_value())
            continue;
        auto &s = db_ss.stations.emplace_back();
        s.marketId = station->marketId;
        s.type = st;
        if (station->bodyId > 0) s.bodyId = station->bodyId;
        if (station->parentBodyId >= 0) s.parentId = station->parentBodyId;
        s.name = station->name;
        s.updated_at = station->updated;

        const auto &ext = station->getStationData();
        s.controllingFaction = ext.controllingFaction;
        s.allegiance = ext.allegiance;
        s.government = ext.government;
        s.state = ext.state;
        s.controllingFactionState = ext.controllingFactionState;
        s.primaryEconomy = ext.primaryEconomy;
        s.secondaryEconomy = ext.secondaryEconomy;
        s.carrierDockingAccess = ext.carrierDockingAccess;
        s.distanceToArrival = ext.distanceToArrival;
        s.latitude = ext.latitude;
        s.longitude = ext.longitude;
        s.landingPads = ext.landingPads;
        s.services = ext.services;
    }

    return true;
}

} // namespace gal

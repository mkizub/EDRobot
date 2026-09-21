//
// Created by mkizub on 21.09.2026.
//

#include "Galaxy.h"
#include "../db/DB.h"

namespace gal {

bool update_star_system(spStarSystem ss, db::StarSystemJS& db_ss) {
    gal::StarSystem::Extra& ext = ss->ext;
    ext.allegiance = db_ss.allegiance;
    ext.government = db_ss.government;
    ext.primaryEconomy = db_ss.primaryEconomy;
    ext.secondaryEconomy = db_ss.secondaryEconomy;
    ext.security = db_ss.security;
    ext.bodyCount = db_ss.bodyCount;
    ext.population = db_ss.population;

    for (auto& body : db_ss.bodies) {
    }

    for (auto& station : db_ss.stations) {
    }

    return true;
}

bool fill_star_system_db(spStarSystem ss, db::StarSystemJS& db_ss) {
    db_ss.id64 = ss->systemAddress;
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

    db_ss.bodies.reserve(ss->bodies.size());
    for (auto &body: ss->bodies) {
        JsBodyType bt;
        switch (body->type) {
        case TypeNav::Star:
            bt = JsBodyType::get("Star");
            break;
        case TypeNav::Planet:
            bt = JsBodyType::get("Planet");
            break;
        case TypeNav::Barycenter:
            bt = JsBodyType::get("Barycentre");
            break;
        case TypeNav::Ring:
            bt = JsBodyType::get("Ring");
            break;
        case TypeNav::AsteroidCluster:
            bt = JsBodyType::get("Asteroid Cluster");
            break;
        default:
            continue;
        }
        auto &b = db_ss.bodies.emplace_back();
        b.type = bt;
        b.bodyId = body->bodyId;
        b.name = body->name;
        b.updated_at = body->updated;

        {
            const auto &ext = body->getBodyData();
            b.subType = ext.subType;
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
            b.tidallyLocked = ext.tidallyLocked;
        }
        if (body->type == TypeNav::Star) {
            const auto &ext = body->getStarData();
            b.ensureStarPart()->mainStar = ext.mainStar;
            b.ensureStarPart()->spectralClass = ext.spectralClass;
            b.ensureStarPart()->luminosity = ext.luminosity;
            b.ensureStarPart()->age = ext.age;
            b.ensureStarPart()->solarRadius = ext.solarRadius;
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
            b.ensurePlanedPart()->radius = ext.radius;
            b.ensurePlanedPart()->earthMasses = ext.earthMasses;
            b.ensurePlanedPart()->gravity = ext.gravity;
            b.ensurePlanedPart()->surfacePressure = ext.surfacePressure;
        }
    }
    db_ss.stations.reserve(ss->stations.size());
    for (auto &station: ss->stations) {
        JsStationType st;
        switch (station->type) {
        case TypeNav::Orbis:
            st = JsStationType::get("Orbis Starport");
            break;
        case TypeNav::Ocellus:
            st = JsStationType::get("Ocellus Starport");
            break;
        case TypeNav::Dodec:
            st = JsStationType::get("Dodec Starport");
            break;
        case TypeNav::Coriolis:
            st = JsStationType::get("Coriolis Starport");
            break;
        case TypeNav::AsteroidBase:
            st = JsStationType::get("Asteroid base");
            break;
        case TypeNav::SpaceOutpost:
            st = JsStationType::get("Outpost");
            break;
        case TypeNav::SpaceInstallation:
            st = JsStationType::get("Space Installation");
            break;
        case TypeNav::SpaceConstrDepot:
            st = JsStationType::get("Space Construction Depot");
            break;
        case TypeNav::Megaship:
            st = JsStationType::get("Mega ship");
            break;
        case TypeNav::StationMegaShip:
            st = JsStationType::get("Station Mega ship");
            break;
        case TypeNav::FleetCarrier:
            st = JsStationType::get("Drake-Class Carrier");
            break;
        case TypeNav::SquadronCarrier:
            st = JsStationType::get("Squadron Carrier");
            break;
        case TypeNav::StrongholdCarrier:
            st = JsStationType::get("Stronghold Carrier");
            break;
        case TypeNav::ColonisationShip:
            st = JsStationType::get("System Colonisation Ship");
            break;
        case TypeNav::PlanetaryPort:
            st = JsStationType::get("Planetary Port");
            break;
        case TypeNav::EngineerPort:
            st = JsStationType::get("Engineer Port");
            break;
        case TypeNav::Settlement:
            st = JsStationType::get("Settlement");
            break;
        case TypeNav::PlanetaryInstallation:
            st = JsStationType::get("Planetary Installation");
            break;
        case TypeNav::PlanetaryConstrDepot:
            st = JsStationType::get("Planetary Construction Depot");
            break;
        default:
            continue;
        }
        auto &s = db_ss.stations.emplace_back();
        s.id = station->marketId;
        s.type = st;
        if (station->bodyId > 0) s.bodyId = station->bodyId;
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

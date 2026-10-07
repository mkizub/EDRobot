//
// Created by mkizub on 01.09.2026.
//

#pragma once

#ifndef EDROBOT_DB_H
#define EDROBOT_DB_H

#include <unordered_set>

namespace db {

constexpr static double DNaN = std::numeric_limits<double>::quiet_NaN();
constexpr static double FNaN = std::numeric_limits<float>::quiet_NaN();
struct StarSystem;
struct StarSystemJS;
struct StationJS;
struct MegashipJS;
struct MarketJS;

enum class CheckPoint {
    PASSIVE, FULL, RESTART, TRUNCATE, NOOP
};
bool checkpoint(CheckPoint mode);

bool init();
bool shutdown();
StarSystem loadStarSystem(std::string_view name);
StarSystem loadStarSystem(int64_t address);
bool loadStarSystemBlob(StarSystemJS& ss, const std::string& system_name, int64_t address);
bool saveStarSystem(const StarSystem& starSystem);
bool saveStarSystemBlob(StarSystemJS& ss, const std::string& system_name, int64_t address);
bool loadMegaShips(int64_t address, std::vector<MegashipJS>& megaships);
bool loadMegaShip(MegashipJS& megaship, bool silent=false);
bool saveMegaShip(MegashipJS& megaship);
bool removeMegaShip(MegashipJS& megaship);
bool loadMarket(MarketJS& market);
bool saveMarket(MarketJS& market);

bool parseSpanshSystemDump(StarSystemJS& ss, const std::string& text);
bool parseSpanshSystemEntry(StarSystemJS& ss, const std::string& text);


bool decode_system_blob(StarSystemJS& ss, const std::string& system_name, int64_t address, const void* data, int size);
bool encode_system_blob(StarSystemJS& ss, const std::string& system_name, int64_t address, std::stringstream& buffer);
bool decode_megaship_blob(StationJS& ss, const void* data, int size);
bool encode_megaship_blob(StationJS& ss, std::stringstream& buffer);
bool decode_market_blob(MarketJS& market, const void* data, int size);
bool encode_market_blob(MarketJS& market, std::stringstream& buffer);

struct Faction {
    int64_t id;
    std::string name;
    JsAllegiance allegiance;
    JsGovernment government;
};

struct FactionStateJS {
    JsFactionState state;
    opt_float trend;
};
struct FactionJS {
    std::string name;
    JsFactionState state;
    JsAllegiance allegiance;
    JsGovernment government;
    opt_float influence;
    js::vector<FactionStateJS> activeStates;
    js::vector<FactionStateJS> pendingStates;
    js::vector<FactionStateJS> recoveringStates;
};

struct StarSystem {
    int64_t id;
    std::string name;
    Coords coords;
    Timestamp updated;
    Timestamp eddn_updated;
};

struct RavenCmdrInfoJS {
    Timestamp timestamp; // contribution timestamp
    int deliveries; // number of deliveries
    int contributed; // total cargo contributed
    js::vector<int64_t> assigned; // commodities assigned to this commander
};

struct SignalJS {
    JsNavType type;
    std::string name;
    std::string var;
    opt_float latitude; // Planetary signals only.
    opt_float longitude; // Planetary signals only.
};

struct MarketLineJS {
    int64_t commodityId;
    unsigned demand;
    unsigned supply;
    unsigned buyPrice;
    unsigned sellPrice;
    opt_bool isConsumer;
    opt_bool isProducer;
};

struct MarketJS {
    int64_t marketId;
    int64_t systemId;
    Timestamp updated;
    std::string stationName;
    std::string stationType;
    std::string starSystem;
    std::string ravenBuildId;
    std::string ravenStatus;
    Timestamp ravenUpdated;
    js::small_map<std::string,RavenCmdrInfoJS> ravenCommanders;
    std::vector<MarketLineJS> commodities;
};

struct StationJS {
    int64_t marketId; // market id
    JsNavType type;
    body_id_t bodyId;  // space stations have bodyId
    body_id_t parentId;  // space stations parent bodyId
    std::string name;
    std::string var;
    Timestamp updated_at;
    std::string realName; // Real name of the station, for colonisation stations.
    std::string carrierName; // Player given name of the station, for fleet carriers.
    js::symbol controllingFaction;
    JsAllegiance allegiance;
    JsGovernment government;
    JsStationState state;
    JsFactionState controllingFactionState;
    JsEconomy primaryEconomy;
    //JsEconomy secondaryEconomy;
    JsCarrierDockingAccess carrierDockingAccess; // Carrier only.
    js::vector<std::pair<JsEconomy,float>> economies;
    opt_float distanceToArrival;
    opt_float latitude; // Planetary stations only.
    opt_float longitude; // Planetary stations only.
    LandingPads landingPads;
    js::vector<JsServices> services;
    // just for reading from spansh
    std::shared_ptr<MarketJS> market;

    void set_economies(js::small_map<JsEconomy,float> map) {
        if (map.empty())
            return;
        economies.reserve(map.size());
        for (auto& [e,p] : map)
            economies.emplace_back(e, p);
        std::sort(economies.begin(), economies.end(), [](auto& e1, auto& e2) -> bool {
            return e1.second > e2.second;
        });
    }
};

struct MegashipJS {
    int64_t systemId;
    int64_t marketId;
    std::string code;
    bool anon;
    std::string name;
    Timestamp updated;
    StationJS blob;
};

struct BodyParentJS {
    JsNavType type;
    int bodyId;
};

struct PowerConflictJS {
    JsPower power;
    float progress;
};

struct ThargoidWarJS {
    JsThargoidState currentState;
    JsThargoidState successState;
    JsThargoidState failureState;
    float progress;
    int daysRemaining;
    int portsRemaining;
    bool successReached;
};

struct StarPartJS {
    opt_bool mainStar;
    int8_t subClass;
    JsLuminosity luminosity;
    uint32_t age; // in millions years
    opt_float solarMasses;
    opt_float absoluteMagnitude;
};
struct PlanetPartJS {
    opt_bool isLandable;
    JsVolcanismType volcanismType;
    JsAtmosphereType atmosphereType;
    JsTerraformingState terraformingState;
    JsReserveLevel reserveLevel;
    opt_float earthMasses;
    opt_float surfaceGravity;
    opt_float surfacePressure;
    js::small_map<JsAtmosphereType,float> atmosphereComposition;
    js::small_map<JsSolidType,float> solidComposition;
    js::small_map<JsMaterials,float> materials;
    // "signals"
};

struct BodyJS {
    JsNavType type;
    body_id_t bodyId;
    std::string name;
    JsBodySubType subType;
    opt_bool tidalLock;
    Timestamp updated_at;
    opt_float radius;
    opt_float distanceToArrival;
    opt_float rotationalPeriod;
    opt_float orbitalPeriod;
    opt_float semiMajorAxis;
    opt_float orbitalEccentricity;
    opt_float orbitalInclination;
    opt_float argOfPeriapsis;
    opt_float meanAnomaly;
    opt_float ascendingNode;
    opt_float surfaceTemperature;
    opt_float axialTilt;

    js::small_map<JsTimestamps,Timestamp> timestamps;
    js::vector<BodyParentJS> parents;
    js::vector<StationJS> stations;
    // "rings"
    // "belts"

    // StarPart accessors
    opt_bool get_mainStar() const { return starPart ? starPart->mainStar : opt_bool{}; }
    void set_mainStar(opt_bool v) { ensureStarPart()->mainStar = v; }

    uint32_t get_age() const { return starPart ? starPart->age : 0; }
    void set_age(uint32_t v) { ensureStarPart()->age = v; }

//    JsSpectralClass get_spectralClass() const { if (starPart) return starPart->spectralClass; return {}; }
//    void set_spectralClass(std::string_view v) {
//        if (v.empty())
//            return;
//        char ch = v[v.size()-1];
//        if (ch >= '0' && ch <= '9')
//            ensureStarPart()->subClass = ch - '0';
//    }

    JsLuminosity get_luminosity() const { if (starPart) return starPart->luminosity; return {}; }
    void set_luminosity(JsLuminosity v) { ensureStarPart()->luminosity = v; }

    opt_float get_absoluteMagnitude() const { return starPart ? starPart->absoluteMagnitude : opt_float{}; }
    void set_absoluteMagnitude(float v) { ensureStarPart()->absoluteMagnitude = v; }

    opt_float get_solarMasses() const { return starPart ? starPart->solarMasses : opt_float{}; }
    void set_solarMasses(float v) { ensureStarPart()->solarMasses = v; }

    opt_float get_solarRadius() const { return radius.has_value() ? radius.value() / 696134.f : opt_float{}; }
    void set_solarRadius(float v) { radius = v * 696134.f; }

    // PlanetPart accessors
    opt_bool get_isLandable() const { return planetPart ? planetPart->isLandable : opt_bool{}; }
    void set_isLandable(opt_bool v) { ensurePlanedPart()->isLandable = v; }

    opt_float get_gravity() const { return planetPart ? planetPart->surfaceGravity : opt_float{}; }
    void set_gravity(float v) { ensurePlanedPart()->surfaceGravity = v; }

    opt_float get_earthMasses() const { return planetPart ? planetPart->earthMasses : opt_float{}; }
    void set_earthMasses(float v) { ensurePlanedPart()->earthMasses = v; }

    opt_float get_radius() const { return radius.has_value() ? radius.value() : opt_float{}; }
    void set_radius(float v) { radius = v; }

    opt_float get_surfacePressure() const { return planetPart ? planetPart->surfacePressure : opt_float{}; }
    void set_surfacePressure(float v) { ensurePlanedPart()->surfacePressure = v; }

    JsVolcanismType get_volcanismType()const  {
        if (planetPart)
            return planetPart->volcanismType;
        return {};
    }
    void set_volcanismType(JsVolcanismType v) {
        ensurePlanedPart()->volcanismType = v;
    }

    JsAtmosphereType get_atmosphereType() const {
        if (planetPart)
            return planetPart->atmosphereType;
        return {};
    }
    void set_atmosphereType(JsAtmosphereType v) {
        ensurePlanedPart()->atmosphereType = v;
    }

    std::optional<js::small_map<JsAtmosphereType,float>> get_atmosphereComposition() const {
        if (planetPart && !planetPart->atmosphereComposition.empty())
            return planetPart->atmosphereComposition;
        return {};
    }
    void set_atmosphereComposition(std::optional<js::small_map<JsAtmosphereType,float>> v) {
        if (v.has_value())
            ensurePlanedPart()->atmosphereComposition = v.value();
    }

    std::optional<js::small_map<JsSolidType,float>> get_solidComposition() const {
        if (planetPart && !planetPart->solidComposition.empty())
            return planetPart->solidComposition;
        return {};
    }
    void set_solidComposition(std::optional<js::small_map<JsSolidType,float>> v) {
        if (v.has_value())
            ensurePlanedPart()->solidComposition = v.value();
    }

    std::optional<js::small_map<JsMaterials,float>> get_materials() const {
        if (planetPart && !planetPart->materials.empty())
            return planetPart->materials;
        return {};
    }
    void set_materials(std::optional<js::small_map<JsMaterials,float>> v) {
        if (v.has_value())
            ensurePlanedPart()->materials = v.value();
    }

    JsTerraformingState get_terraformingState() const {
        if (planetPart)
            return planetPart->terraformingState;
        return {};
    }
    void set_terraformingState(JsTerraformingState v) {
        ensurePlanedPart()->terraformingState = v;
    }

    JsReserveLevel get_reserveLevel() const {
        if (planetPart)
            return planetPart->reserveLevel;
        return {};
    }
    void set_reserveLevel(JsReserveLevel v) {
        ensurePlanedPart()->reserveLevel = v;
    }

    StarPartJS* getStarPart() const { return starPart.get(); }
    PlanetPartJS* getPlanetPart() const { return planetPart.get(); }
    StarPartJS* ensureStarPart() {
        if (!starPart)
            starPart = std::make_unique<StarPartJS>();
        return starPart.get();
    }
    PlanetPartJS* ensurePlanedPart() {
        if (!planetPart)
            planetPart = std::make_unique<PlanetPartJS>();
        return planetPart.get();
    }

private:
    std::unique_ptr<StarPartJS> starPart;
    std::unique_ptr<PlanetPartJS> planetPart;
};

struct StarSystemJS {
    int64_t address;
    std::string name;
    Coords coords;
    JsAllegiance allegiance;
    JsGovernment government;
    JsEconomy primaryEconomy;
    JsEconomy secondaryEconomy;
    JsSecurity security;
    uint16_t bodyCount;
    uint64_t population;
    Timestamp updated_at;
    std::unique_ptr<FactionJS> controllingFaction;
    js::vector<FactionJS> factions;
    JsPowerState powerState;
    js::vector<PowerConflictJS> powerConflictProgress;
    js::vector<JsPower> powers;
    JsPower controllingPower;
    opt_float powerStateControlProgress;
    opt_float powerStateReinforcement;
    opt_float powerStateUndermining;
    std::unique_ptr<ThargoidWarJS> thargoidWar;
    js::small_map<JsTimestamps,Timestamp> timestamps;
    js::vector<BodyJS> bodies;
    js::vector<StationJS> stations;
    js::vector<SignalJS> signals;
};

} // namespace db


#endif //EDROBOT_DB_H

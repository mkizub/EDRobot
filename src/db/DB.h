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

bool init();
bool shutdown();
StarSystem loadStarSystem(std::string_view name);
StarSystem loadStarSystem(int64_t address);
bool loadStarSystemBlob(StarSystemJS& ss, const std::string& system_name, int64_t address);
bool saveStarSystem(const StarSystem& starSystem);
bool saveStarSystemBlob(StarSystemJS& ss, const std::string& system_name, int64_t address);

bool decode_system_blob(StarSystemJS& ss, const std::string& system_name, int64_t address, const void* data, int size);
bool encode_system_blob(StarSystemJS& ss, const std::string& system_name, int64_t address, std::stringstream& buffer);

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
    double x, y, z;
    Timestamp updated;
    Timestamp eddn_updated;
};

struct MarketLineJS {
    int64_t id {};
    unsigned demand {};
    unsigned supply {};
    unsigned buyPrice {};
    unsigned sellPrice {};
};

struct StationJS {
    int64_t id; // market id
    JsStationType type;
    int bodyId {};  // space stations have bodyId
    std::string name;
    Timestamp updated_at;
    std::string realName; // Real name of the station, for colonisation stations.
    std::string carrierName; // Player given name of the station, for fleet carriers.
    js::symbol controllingFaction;
    JsAllegiance allegiance;
    JsGovernment government;
    JsStationState state;
    JsFactionState controllingFactionState;
    JsEconomy primaryEconomy;
    JsEconomy secondaryEconomy;
    JsCarrierDockingAccess carrierDockingAccess; // Carrier only.
    js::small_map<JsEconomy,float> economies;
    opt_float distanceToArrival;
    opt_float latitude; // Planetary stations only.
    opt_float longitude; // Planetary stations only.
    LandingPads landingPads;
    js::vector<JsServices> services;
};

struct CoordsJS {
    double x {};
    double y {};
    double z {};
};

struct BodyParentJS {
    JsParentBodyType type;
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
    bool mainStar;
    JsSpectralClass spectralClass;
    JsLuminosity luminosity;
    uint32_t age; // in millions years
    opt_float solarRadius;
    opt_float solarMasses;
    opt_float absoluteMagnitude;
};
struct PlanetPartJS {
    bool isLandable;
    JsVolcanismType volcanismType;
    JsAtmosphereType atmosphereType;
    JsTerraformingState terraformingState;
    JsReserveLevel reserveLevel;
    opt_float radius;
    opt_float earthMasses;
    opt_float gravity;
    opt_float surfacePressure;
    js::small_map<JsAtmosphereType,float> atmosphereComposition;
    js::small_map<JsSolidType,float> solidComposition;
    js::small_map<JsMaterials,float> materials;
    // "signals"
};

struct BodyJS {
    JsBodyType type;
    int bodyId {};
    std::string name;
    JsBodySubType subType;
    Timestamp updated_at;
    opt_float orbitalPeriod;
    opt_float semiMajorAxis;
    opt_float orbitalEccentricity;
    opt_float orbitalInclination;
    opt_float argOfPeriapsis;
    opt_float meanAnomaly;
    opt_float ascendingNode;
    opt_float distanceToArrival;
    opt_float surfaceTemperature;
    opt_float rotationalPeriod;
    opt_float axialTilt;
    bool tidallyLocked {};

    js::small_map<JsTimestamps,Timestamp> timestamps;
    js::vector<BodyParentJS> parents;
    js::vector<StationJS> stations;
    // "rings"
    // "belts"

    // StarPart accessors
    bool get_mainStar() const { return starPart && starPart->mainStar; }
    void set_mainStar(bool v) { ensureStarPart()->mainStar = v; }

    uint64_t get_age() const { return starPart ? starPart->age : 0; }
    void set_age(uint64_t v) { ensureStarPart()->age = v; }

    JsSpectralClass get_spectralClass() const { if (starPart) return starPart->spectralClass; return {}; }
    void set_spectralClass(JsSpectralClass v) { ensureStarPart()->spectralClass = v; }

    JsLuminosity get_luminosity() const { if (starPart) return starPart->luminosity; return {}; }
    void set_luminosity(JsLuminosity v) { ensureStarPart()->luminosity = v; }

    opt_float get_absoluteMagnitude() const { return starPart ? starPart->absoluteMagnitude : opt_float{}; }
    void set_absoluteMagnitude(float v) { ensureStarPart()->absoluteMagnitude = v; }

    opt_float get_solarMasses() const { return starPart ? starPart->solarMasses : opt_float{}; }
    void set_solarMasses(float v) { ensureStarPart()->solarMasses = v; }

    opt_float get_solarRadius() const { return starPart ? starPart->solarRadius : opt_float{}; }
    void set_solarRadius(float v) { ensureStarPart()->solarRadius = v; }

    // PlanetPart accessors
    bool get_isLandable() const { return planetPart && planetPart->isLandable; }
    void set_isLandable(bool v) { ensurePlanedPart()->isLandable = v; }

    opt_float get_gravity() const { return planetPart ? planetPart->gravity : opt_float{}; }
    void set_gravity(float v) { ensurePlanedPart()->gravity = v; }

    opt_float get_earthMasses() const { return planetPart ? planetPart->earthMasses : opt_float{}; }
    void set_earthMasses(float v) { ensurePlanedPart()->earthMasses = v; }

    opt_float get_radius() const { return planetPart ? planetPart->radius : opt_float{}; }
    void set_radius(float v) { ensurePlanedPart()->radius = v; }

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
    int64_t id64 {};
    std::string name;
    CoordsJS coords {};
    JsAllegiance allegiance;
    JsGovernment government;
    JsEconomy primaryEconomy {};
    JsEconomy secondaryEconomy {};
    JsSecurity security {};
    uint16_t bodyCount {};
    uint64_t population {};
    Timestamp updated_at {};
    std::unique_ptr<FactionJS> controllingFaction;
    js::vector<FactionJS> factions;
    JsPowerState powerState {};
    js::vector<PowerConflictJS> powerConflictProgress;
    js::vector<JsPower> powers;
    JsPower controllingPower {};
    opt_float powerStateControlProgress;
    opt_float powerStateReinforcement;
    opt_float powerStateUndermining;
    std::unique_ptr<ThargoidWarJS> thargoidWar {};
    js::small_map<JsTimestamps,Timestamp> timestamps;
    js::vector<BodyJS> bodies;
    js::vector<StationJS> stations;
};

} // namespace db


#include <glaze/forward.hpp>

template <js::IsEnum E>
struct glz::meta<E>
{
    static constexpr bool custom_write = true;
    static constexpr bool custom_read = true;
};

template <>
struct glz::meta<Timestamp>
{
    static constexpr bool custom_write = true;
    static constexpr bool custom_read = true;
};

#endif //EDROBOT_DB_H

//
// Created by mkizub on 01.09.2026.
//

#pragma once

#ifndef EDROBOT_DB_H
#define EDROBOT_DB_H

#include <unordered_set>
#include "../small_map.hpp"
#include "JsEnum.h"

namespace db {

constexpr static double DNaN = std::numeric_limits<double>::quiet_NaN();
constexpr static double FNaN = std::numeric_limits<float>::quiet_NaN();


void init_js_remapping();

bool init();
bool shutdown();
struct StarSystem loadStarSystem(std::string_view name);
struct StarSystem loadStarSystem(int64_t address);
bool saveStarSystem(const StarSystem& starSystem);
//enum class JsBlobTable : int {
//    Systems = 1,
//    Bodies = 2,
//    Stations = 3
//};
//void loadJsBlobs(int64_t blobId);


struct Faction {
    int64_t id;
    std::string name;
    JsEnum<JsAllegiance> allegiance;
    JsEnum<JsGovernment> government;
};

struct FactionStateJS {
    JsEnum<JsFactionState> state;
    std::optional<double> trend;
};
struct FactionJS {
    std::string name;
    JsEnum<JsFactionState> state;
    JsEnum<JsAllegiance> allegiance;
    JsEnum<JsGovernment> government;
    std::optional<double> influence;
    std::vector<FactionStateJS> activeStates;
    std::vector<FactionStateJS> pendingStates;
    std::vector<FactionStateJS> recoveringStates;
};

struct StarSystem {
    int64_t id;
    std::string name;
    double x, y, z;
    int64_t population;
    Faction* faction; // controllingFaction
    int64_t blobId;
};

struct CoordsJS {
    double x {};
    double y {};
    double z {};
};
struct PowerConflictJS {
    JsEnum<JsPower> power;
    double progress;
};
struct ThargoidWarJS {
    JsEnum<JsThargoidState> currentState;
    JsEnum<JsThargoidState> successState;
    JsEnum<JsThargoidState> failureState;
    double progress;
    int daysRemaining;
    int portsRemaining;
    bool successReached;
};
struct StarPartJS {
    bool mainStar;
    int64_t age;
    std::string spectralClass;
    std::string luminosity;
    float absoluteMagnitude {FNaN};
    float solarMasses {FNaN};
    float solarRadius {FNaN};
};
struct PlanetPartJS {
    bool isLandable;
    float gravity {FNaN};
    float earthMasses {FNaN};
    float radius {FNaN};
    float surfacePressure {FNaN};
    JsEnum<JsVolcanismType> volcanismType;
    JsEnum<JsAtmosphereType> atmosphereType;
    ed::small_map<JsEnum<JsAtmosphereType>,float> atmosphereComposition;
    ed::small_map<JsEnum<JsSolidType>,float> solidComposition;
    JsEnum<JsTerraformingState> terraformingState;
    ed::small_map<JsEnum<JsMaterials>,float> materials;
    JsEnum<JsReserveLevel> reserveLevel;
    // "signals"
};

struct BodyJS {
    JsEnum<JsBodyType> type;
    int bodyId {};
    std::string name;
    JsEnum<JsBodySubType> subType;
    float orbitalPeriod {FNaN};
    float semiMajorAxis {FNaN};
    float orbitalEccentricity {FNaN};
    float orbitalInclination {FNaN};
    float argOfPeriapsis {FNaN};
    float meanAnomaly {FNaN};
    float ascendingNode {FNaN};
    float distanceToArrival {FNaN};
    float surfaceTemperature {FNaN};
    float rotationalPeriod {FNaN};
    float axialTilt {FNaN};
    bool rotationalPeriodTidallyLocked {};

    ed::small_map<JsEnum<JsTimestamps>,Timestamp> timestamps;
    Timestamp updated_at;
    std::vector<ed::small_map<JsEnum<JsParentBodyType>,int>> parents;
    // stations
    // "rings"
    // "belts"


    // StarPart accessors
    bool get_mainStar() const { return starPart && starPart->mainStar; }
    void set_mainStar(bool v) { ensureStarPart()->mainStar = v; }

    int64_t get_age() const { return starPart ? starPart->age : 0; }
    void set_age(int64_t v) { ensureStarPart()->age = v; }

    std::string_view get_spectralClass() const { if (starPart) return starPart->spectralClass; return {}; }
    void set_spectralClass(std::string_view v) { ensureStarPart()->spectralClass = v; }

    std::string_view get_luminosity() const { if (starPart) return starPart->luminosity; return {}; }
    void set_luminosity(std::string_view v) { ensureStarPart()->luminosity = v; }

    float get_absoluteMagnitude() const { return starPart ? starPart->absoluteMagnitude : FNaN; }
    void set_absoluteMagnitude(float v) { ensureStarPart()->absoluteMagnitude = v; }

    float get_solarMasses() const { return starPart ? starPart->solarMasses : FNaN; }
    void set_solarMasses(float v) { ensureStarPart()->solarMasses = v; }

    float get_solarRadius() const { return starPart ? starPart->solarRadius : FNaN; }
    void set_solarRadius(float v) { ensureStarPart()->solarRadius = v; }

    // PlanetPart accessors
    bool get_isLandable() const { return planetPart && planetPart->isLandable; }
    void set_isLandable(bool v) { ensurePlanedPart()->isLandable = v; }

    float get_gravity() const { return planetPart ? planetPart->gravity : FNaN; }
    void set_gravity(float v) { ensurePlanedPart()->gravity = v; }

    float get_earthMasses() const { return planetPart ? planetPart->earthMasses : FNaN; }
    void set_earthMasses(float v) { ensurePlanedPart()->earthMasses = v; }

    float get_radius() const { return planetPart ? planetPart->radius : FNaN; }
    void set_radius(float v) { ensurePlanedPart()->radius = v; }

    float get_surfacePressure() const { return planetPart ? planetPart->surfacePressure : FNaN; }
    void set_surfacePressure(float v) { ensurePlanedPart()->surfacePressure = v; }

    std::optional<JsEnum<JsVolcanismType>> get_volcanismType()const  {
        if (planetPart && planetPart->volcanismType)
            return planetPart->volcanismType;
        return {};
    }
    void set_volcanismType(std::optional<JsEnum<JsVolcanismType>> v) {
        if (v.has_value())
            ensurePlanedPart()->volcanismType = v.value();
    }

    std::optional<JsEnum<JsAtmosphereType>> get_atmosphereType() const {
        if (planetPart && planetPart->atmosphereType)
            return planetPart->atmosphereType;
        return {};
    }
    void set_atmosphereType(std::optional<JsEnum<JsAtmosphereType>> v) {
        if (v.has_value())
            ensurePlanedPart()->atmosphereType = v.value();
    }

    std::optional<ed::small_map<JsEnum<JsAtmosphereType>,float>> get_atmosphereComposition() const {
        if (planetPart && !planetPart->atmosphereComposition.empty())
            return planetPart->atmosphereComposition;
        return {};
    }
    void set_atmosphereComposition(std::optional<ed::small_map<JsEnum<JsAtmosphereType>,float>> v) {
        if (v.has_value())
            ensurePlanedPart()->atmosphereComposition = v.value();
    }

    std::optional<ed::small_map<JsEnum<JsSolidType>,float>> get_solidComposition() const {
        if (planetPart && !planetPart->solidComposition.empty())
            return planetPart->solidComposition;
        return {};
    }
    void set_solidComposition(std::optional<ed::small_map<JsEnum<JsSolidType>,float>> v) {
        if (v.has_value())
            ensurePlanedPart()->solidComposition = v.value();
    }

    std::optional<ed::small_map<JsEnum<JsMaterials>,float>> get_materials() const {
        if (planetPart && !planetPart->materials.empty())
            return planetPart->materials;
        return {};
    }
    void set_materials(std::optional<ed::small_map<JsEnum<JsMaterials>,float>> v) {
        if (v.has_value())
            ensurePlanedPart()->materials = v.value();
    }

    std::optional<JsEnum<JsTerraformingState>> get_terraformingState() const {
        if (planetPart && planetPart->terraformingState)
            return planetPart->terraformingState;
        return {};
    }
    void set_terraformingState(std::optional<JsEnum<JsTerraformingState>> v) {
        if (v.has_value())
            ensurePlanedPart()->terraformingState = v.value();
    }

    std::optional<JsEnum<JsReserveLevel>> get_reserveLevel() const {
        if (planetPart && planetPart->reserveLevel)
            return planetPart->reserveLevel;
        return {};
    }
    void set_reserveLevel(std::optional<JsEnum<JsReserveLevel>> v) {
        if (v.has_value())
            ensurePlanedPart()->reserveLevel = v.value();
    }

    StarPartJS* getStarPart() { return starPart.get(); }
    PlanetPartJS* getPlanetPart() { return planetPart.get(); }
private:
    std::unique_ptr<StarPartJS> starPart;
    std::unique_ptr<PlanetPartJS> planetPart;

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
};

struct StarSystemJS {
    int64_t id64 {};
    std::string name;
    CoordsJS coords {};
    JsEnum<JsAllegiance> allegiance;
    JsEnum<JsGovernment> government;
    JsEnum<JsEconomy> primaryEconomy {};
    JsEnum<JsEconomy> secondaryEconomy {};
    JsEnum<JsSecurity> security {};
    uint64_t population {};
    uint16_t bodyCount {};
    Timestamp updated_at {};
    std::unique_ptr<FactionJS> controllingFaction;
    std::vector<FactionJS> factions;
    JsEnum<JsPowerState> powerState {};
    std::vector<PowerConflictJS> powerConflictProgress;
    std::vector<JsEnum<JsPower>> powers;
    JsEnum<JsPower> controllingPower {};
    std::optional<double> powerStateControlProgress;
    std::optional<double> powerStateReinforcement;
    std::optional<double> powerStateUndermining;
    std::unique_ptr<ThargoidWarJS> thargoidWar {};
    ed::small_map<JsEnum<JsTimestamps>,Timestamp> timestamps;
    std::vector<BodyJS> bodies;
};

}

#endif //EDROBOT_DB_H

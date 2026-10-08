#include <utility>

//
// Created by mkizub on 27.09.2026.
//

#pragma once

#ifndef EDROBOT_STARSYSTEM_H
#define EDROBOT_STARSYSTEM_H

namespace gal {

struct StarSystem;

struct StarSystem {
    friend class StarSystemCache;
    friend std::scoped_lock<StarSystem>;
private:
    StarSystem(int64_t address, std::string_view name, Coords coords)
            : systemAddress(address)
            , systemName(name)
            , systemCoords(coords)
    {}

    struct EntitiesLocker {
        const StarSystem * const ss;
        explicit EntitiesLocker(const StarSystem* ss) noexcept : ss(ss) {
            bool expected = false;
            while (!ss->entitiesLocked.compare_exchange_strong(expected, true))
                ss->entitiesLocked.wait(false);
        }
        ~EntitiesLocker() noexcept {
            ss->entitiesLocked.store(false);
            ss->entitiesLocked.notify_one();
        }
    };
    friend struct EntitiesLocker;

public:
    virtual ~StarSystem() = default;
    const int64_t systemAddress;
    const std::string systemName;
    const Coords systemCoords;
    Timestamp updated_at {};
    Timestamp eddn_updated_at {};

    struct Extra {
        JsAllegiance allegiance;
        JsGovernment government;
        JsEconomy primaryEconomy {};
        JsEconomy secondaryEconomy {};
        JsSecurity security {};
        uint16_t bodyCount {};
        uint64_t population {};
    } ext;

private:
    std::vector<spEntity> entities;

public:
    template <typename F>
    void forEach(F lambda) const {
        EntitiesLocker lock(this);
        for (auto& eRef : entities) {
            if (eRef)
                lambda(eRef);
        }
    }
    template <typename F>
    int countEntities(F lambda) const {
        EntitiesLocker lock(this);
        int count = 0;
        for (auto& eRef : entities) {
            if (eRef && lambda(eRef))
                count += 1;
        }
        return count;
    }
    std::vector<spEntity> getEntitiesSnapshot() const {
        EntitiesLocker lock(this);
        std::vector<spEntity> snapshot;
        snapshot.reserve(entities.size());
        for (auto& eRef : entities) {
            if (eRef)
                snapshot.push_back(eRef);
        }
        return snapshot;
    }

    bool needCoreSave {false};
    bool isBlobLoaded {false};
    bool needBlobSave {false};
private:
    mutable std::atomic_bool entitiesLocked;
public:
    void load();
    void save();
    void wipe();

    bool has_entities() const { return !entities.empty(); }

    struct entity_args {
        int64_t market_id;
        body_id_t body_id;
        Timestamp ts;
        bool manual;
    };

    spEntity getMainStar() const;
    spEntity getEntity(const std::string& bname) const;
    spEntity getSignal(std::string_view sig_name) const;
    spEntity getBodyById(body_id_t bodyId) const;
    spEntity getBody(std::string_view bname) const;
    spEntity getDock(std::string_view sname) const;
    spEntity getDock(int64_t marketId) const;
    spEntity addNavListEntry(wchar_t charOCR, const std::string& nav_icon, const std::string& name, body_id_t bodyId);
    spEntity makeSignalUSS(std::string_view sname, std::string_view snloc, std::string_view uss_type, std::string_view uss_tloc);
    spEntity addEntity(TypeNav tp, std::string_view name, std::string_view nloc, entity_args args);
    void removeEntity(const spEntity& entity);
    spEntity moveMegashipHere(int64_t marketId, std::string_view code, std::string_view name, body_id_t parentId, bool silent);
    spEntity checkTypeAndName(spEntity entity, TypeNav tp, std::string_view sname, std::string_view snloc, entity_args args);

private:
    spEntity moveMegashipHere(db::MegashipJS& db_ms, body_id_t parentId);
    spEntity makeStationEntity(TypeNav tp, std::string_view name, std::string_view nloc, int64_t marketId, Timestamp ts);
    spEntity makeSignalEntity(TypeNav tp, std::string_view name, std::string_view nloc);
    spEntity addMegashipEntity(TypeNav tp, std::string_view name, std::string_view nloc, int64_t marketId, Timestamp ts);
    spEntity addMegashipEntity(TypeNav tp, db::MegashipJS& db_ms, std::string_view nloc);
    spEntity addEntity(spEntity entity); // TODO: rename, public api is addEntity(type,name);
};

struct SignalData {
    js::symbol unexpanded_name; // "$MULTIPLAYER_SCENARIO42_TITLE;"
    js::symbol localised_name; // "Нав. маяк"
    bool localised; // whether unexpanded_name was localised or not
    int count {}; // how many signals of this type in this system or on this planet
    //opt_float distanceToArrival; // space signals
    opt_float latitude; // planetary signals
    opt_float longitude; // planetary signals
};
struct BodyData {
    opt_bool  tidalLock;
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
};
struct StarData {
    opt_bool mainStar;
    int8_t subClass {-1};
    JsBodySubType subType;
    JsLuminosity luminosity;
    uint32_t age {}; // in millions years
    opt_float solarMasses;
    opt_float absoluteMagnitude;
};
struct PlanetData {
    opt_bool isLandable;
    JsBodySubType subType;
    JsVolcanismType volcanismType;
    JsAtmosphereType atmosphereType;
    JsTerraformingState terraformingState;
    JsReserveLevel reserveLevel;
    opt_float earthMasses;
    opt_float surfaceGravity;
    opt_float surfacePressure;
};
struct StationData {
    int64_t marketId {0};
    js::symbol unexpanded_name; // "$Ancient:#index=3;" or "$EXT_PANEL_ColonisationShip; Chretien Depot"
    js::symbol localised_name; // full name for carriers, or localised name from unexpanded_name or from Entity::name
    js::symbol controllingFaction;
    bool localised; // whether unexpanded_name was localised or not
    JsAllegiance allegiance;
    JsGovernment government;
    JsStationState state;
    JsFactionState controllingFactionState;
    JsEconomy primaryEconomy;
    JsCarrierDockingAccess carrierDockingAccess; // Carrier only.
    js::vector<std::pair<JsEconomy,float>> economies;
    opt_float distanceToArrival;
    opt_float latitude; // Planetary stations only.
    opt_float longitude; // Planetary stations only.
    LandingPads landingPads {};
    js::vector<JsServices> services;
};

class Entity {
    friend StarSystem;
protected:
    Entity(TypeNav tp, std::string name);
    Entity(TypeNav tp, std::string name, body_id_t bId);
public:
    virtual ~Entity() = default;
    [[nodiscard]] virtual bool nameEq(std::string_view nm) const; // check name, unexpanded_name and localised_name
    const TypeNav type {TypeNav::Other};
    body_id_t bodyId;
    body_id_t parentBodyId; // planet for planetary stations, or on orbit of body
    Timestamp updated {};
    const std::string name; // english name, code for carriers

    [[nodiscard]] float radius() const {
        if (!isBody(type))
            return 0;
        auto* bd = getBodyData();
        return bd ? bd->radius.value_or(0) : 0;
    }

    [[nodiscard]] int64_t marketId() const {
        auto* sd = getStationData();
        return sd ? sd->marketId : 0;
    }

    virtual bool setMarketId(int64_t marketId) {
        return false;
    }

    virtual std::string_view getUnexpandedName() const { return name; }
    virtual std::string_view getLocalisedName() const { return ""; }

    //virtual bool setTypeAndName(TypeNav js_nt, std::string_view name, std::string_view nloc = {});

    // StationData if: type >= TypeNav::SpaceStation
    // PlanetData if: type == TypeNav::Planet
    // StarData if: type == TypeNav::Star
    // BodyData: all bodies (including bodies, signals, beacons, etc)
    // SignalData: all signals (including generic signals, beacons, codex entries, etc)
    [[nodiscard]] virtual SignalData* getSignalData() { return nullptr; }
    [[nodiscard]] virtual const SignalData* getSignalData() const { return nullptr; }
    [[nodiscard]] virtual BodyData* getBodyData() { return nullptr; }
    [[nodiscard]] virtual const BodyData* getBodyData() const { return nullptr; }
    [[nodiscard]] virtual StarData* getStarData() { return nullptr; }
    [[nodiscard]] virtual const StarData* getStarData() const { return nullptr; }
    [[nodiscard]] virtual PlanetData* getPlanetData() { return nullptr; }
    [[nodiscard]] virtual const PlanetData* getPlanetData() const { return nullptr; }
    [[nodiscard]] virtual StationData* getStationData() { return nullptr; }
    [[nodiscard]] virtual const StationData* getStationData() const { return nullptr; }

private:
    // only StarSystem can set name (and localized name)
    //virtual bool setName(std::string_view nm, std::string_view nl);
};

class SignalEntity : public Entity {
    friend StarSystem;
protected:
    SignalEntity(TypeNav tp, std::string var, std::string name, std::string nloc);
protected:
    SignalData signal_ext;
    [[nodiscard]] SignalData* getSignalData() override { return &signal_ext; }
    [[nodiscard]] const SignalData* getSignalData() const override { return &signal_ext; }
    [[nodiscard]] bool nameEq(std::string_view nm) const override;
    std::string_view getUnexpandedName() const { return signal_ext.unexpanded_name.sv(); }
    std::string_view getLocalisedName() const { return signal_ext.localised_name.sv(); }
    static bool setSignalName(spEntity& entity, std::string_view nm, std::string_view nl);
};

class SignalUUSEntity : public SignalEntity {
    friend StarSystem;
    SignalUUSEntity(std::string nm, std::string nl)
        : SignalEntity(TypeNav::USS, "", std::move(nm), std::move(nl))
    {}
public:
};

class BodyEntity : public Entity {
    friend StarSystem;
protected:
    BodyEntity(TypeNav tp, std::string nm, body_id_t id)
        : Entity(tp, std::move(nm), std::move(id))
    {}
public:
//    static spEntity create(TypeNav tp, std::string_view sname, std::string_view snloc = {});
protected:
    BodyData body_ext;
    [[nodiscard]] BodyData* getBodyData() override { return &body_ext; }
    [[nodiscard]] const BodyData* getBodyData() const override { return &body_ext; }
    static bool setBodyName(spEntity& entity, std::string_view nm);
};

class StarEntity : public BodyEntity {
    friend StarSystem;
    StarEntity(std::string nm, body_id_t id)
        : BodyEntity(TypeNav::Star, std::move(nm), std::move(id))
    {}
protected:
    StarData star_ext;
    [[nodiscard]] StarData* getStarData() override { return &star_ext; }
    [[nodiscard]] const StarData* getStarData() const override { return &star_ext; }
};

class PlanetEntity : public BodyEntity {
    friend StarSystem;
    PlanetEntity(std::string nm, body_id_t id)
        : BodyEntity(TypeNav::Planet, std::move(nm), std::move(id))
    {}
protected:
    PlanetData planet_ext;
    [[nodiscard]] PlanetData* getPlanetData() override { return &planet_ext; }
    [[nodiscard]] const PlanetData* getPlanetData() const override { return &planet_ext; }
};

class StationEntity : public Entity {
    friend StarSystem;
protected:
    StationEntity(TypeNav tp, std::string nm, int64_t market_id);
    StationEntity(TypeNav tp, std::string var, std::string nm, std::string nl, int64_t market_id);
protected:
    bool setMarketId(int64_t marketId) override;
    StationData station_ext;
    [[nodiscard]] StationData* getStationData() override { return &station_ext; }
    [[nodiscard]] const StationData* getStationData() const override { return &station_ext; }
    [[nodiscard]] bool nameEq(std::string_view nm) const override;
    std::string_view getUnexpandedName() const { return station_ext.unexpanded_name.sv(); }
    std::string_view getLocalisedName() const { return station_ext.localised_name.sv(); }
    static bool setStationName(spEntity& entity, std::string_view nm, std::string_view nl);
};

class MegashipEntity : public StationEntity {
    friend StarSystem;
protected:
    MegashipEntity(TypeNav tp, std::string var, std::string nm, std::string nl, int64_t market_id);
    bool setMarketId(int64_t marketId) override;
    static bool setCarrierName(spEntity& entity, std::string_view code, std::string_view full);
    static bool setMegashipName(spEntity& entity, std::string_view nm, std::string_view nl);
    [[nodiscard]] bool nameEq(std::string_view nm) const override;
};


} // namespace gal

#endif //EDROBOT_STARSYSTEM_H

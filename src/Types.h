//
// Created by mkizub on 31.08.2025.
//

#ifndef EDROBOT_TYPES_H
#define EDROBOT_TYPES_H

typedef std::chrono::time_point<std::chrono::utc_clock> Timestamp;

enum class WState : int { Unknown=-1, Normal=0, Focused=1, Active=2, Disabled=3 };

enum class Lang { XX=-1, EN=0, RU=1 };

enum class GuiFocus { None=0, Right=1, Left=2, Chat=3, Role=4, Services=5, GalaxyMap=6, SystemMap=7, Orrery=8, FSS=9, SAA=10, Codex=11 };



namespace gal {

enum class TypeNav {
    Other, // space barycentre, asteroid cluster, etc.
    Star,
    Planet,
    SpacePort, // Orbis, Ocellus, Coriolis, AsteroidBase - large landing pads
    SpaceInst, // non-dockable space installations / instance
    SpaceConstr, // space construction site
    PlanetPort, // planet ports - large landing pads
    PlanetInst, // non-dockable ground installations / instance
    PlanetConstr, // planet construction site
    Carrier, // pilot/squadron fleet carrier
    MegashipDock, // dockable megaship - pp base, trailblazer megaships
    MegashipInst, // non-dockable space megaship / instance
};

enum class TypeSite {
    Other,
    Orbis,
    Ocellus,
    Coriolis,
    AsteroidBase,
    SpaceOutpost,
    FleetCarrier,
    SquadronCarrier,
    StrongholdCarrier,
    ColonisationShip,
    StationMegaShip,
    TrailblazerDream,
    EngineerPort,
    Settlement, // odyssey settlements
    NavBeacon,
    SpaceConstr,
};

} // namespace gal

namespace ai {

void toggleDebugPause();

}

struct dist_t {
    enum Unit {
        X, M, KM, MM, LS, LY
    } unit;
    double dist;

    dist_t() : unit(X), dist(0) {}
    dist_t(Unit u, double d) : unit(u), dist(d) {}

    bool valid() const { return unit != X; }
    dist_t convertTo(Unit u) const;
    double get(Unit u);
    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const dist_t& obj);
};

struct utc_timer {
    std::chrono::time_point<std::chrono::utc_clock> time_start;
    std::chrono::time_point<std::chrono::utc_clock> time_limit;
    utc_timer() = default;
    utc_timer(std::chrono::seconds seconds) {
        time_start = std::chrono::utc_clock::now();
        time_limit = time_start + seconds;
    }
    bool expired();
    int sec_passed();
    int sec_left();
    std::string passed();
    std::string left();
};

#endif //EDROBOT_TYPES_H

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
    PlanetConstr,
};

} // namespace gal

namespace ai {

void toggleDebugPause();
bool isDebugPause();

}

struct dist_t {
    enum Unit {
        X, M, KM, MM, LS, LY
    } unit;
    double dist;

    constexpr dist_t() : unit(X), dist(0) {}
    constexpr dist_t(Unit u, double d) : unit(u), dist(d) {}

    bool valid() const { return unit != X; }
    dist_t convertTo(Unit u) const;
    double get(Unit u) const;
    std::string to_string() const;
};
std::ostream& operator<<(std::ostream& os, const dist_t& obj);

constexpr inline dist_t operator""_m(uint64_t val) noexcept { return dist_t(dist_t::M, val); }
constexpr inline dist_t operator""_m(long double val) noexcept { return dist_t(dist_t::M, val); }
constexpr inline dist_t operator""_km(uint64_t val) noexcept { return dist_t(dist_t::KM, val); }
constexpr inline dist_t operator""_km(long double val) noexcept { return dist_t(dist_t::KM, val); }
constexpr inline dist_t operator""_Mm(uint64_t val) noexcept { return dist_t(dist_t::MM, val); }
constexpr inline dist_t operator""_Mm(long double val) noexcept { return dist_t(dist_t::MM, val); }
constexpr inline dist_t operator""_ls(uint64_t val) noexcept { return dist_t(dist_t::LS, val); }
constexpr inline dist_t operator""_ls(long double val) noexcept { return dist_t(dist_t::LS, val); }
constexpr inline dist_t operator""_ly(uint64_t val) noexcept { return dist_t(dist_t::LY, val); }
constexpr inline dist_t operator""_ly(long double val) noexcept { return dist_t(dist_t::LY, val); }

inline bool operator ==(const dist_t d1, const dist_t d2) {
    return d1.valid() && d2.valid() && d1.get(dist_t::M) == d2.get(dist_t::M);
}
inline bool operator <(const dist_t d1, const dist_t d2) {
    return d1.valid() && d2.valid() && d1.get(dist_t::M) < d2.get(dist_t::M);
}
inline bool operator >(const dist_t d1, const dist_t d2) {
    return d1.valid() && d2.valid() && d1.get(dist_t::M) > d2.get(dist_t::M);
}
inline bool operator <=(const dist_t d1, const dist_t d2) {
    return d1.valid() && d2.valid() && d1.get(dist_t::M) <= d2.get(dist_t::M);
}
inline bool operator >=(const dist_t d1, const dist_t d2) {
    return d1.valid() && d2.valid() && d1.get(dist_t::M) >= d2.get(dist_t::M);
}
inline dist_t operator +(const dist_t d1, const dist_t d2) {
    dist_t::Unit u = d1.unit < d2.unit ? d1.unit : d2.unit;
    return {u, d1.get(u) + d2.get(u)};
}
inline dist_t operator -(const dist_t d1, const dist_t d2) {
    dist_t::Unit u = d1.unit < d2.unit ? d1.unit : d2.unit;
    return {u, d1.get(u) + d2.get(u)};
}
inline dist_t operator *(const dist_t d, double scale) {
    return {d.unit, d.dist * scale};
}
inline dist_t operator /(const dist_t d, double scale) {
    return {d.unit, d.dist * scale};
}

struct utc_timer {
    std::chrono::time_point<std::chrono::utc_clock> time_start;
    std::chrono::time_point<std::chrono::utc_clock> time_limit;
    utc_timer() = default;
    utc_timer(std::chrono::seconds seconds) {
        time_start = std::chrono::utc_clock::now();
        time_limit = time_start + seconds;
    }
    bool started() const;
    bool expired() const;
    int sec_passed() const;
    int sec_left() const;
    std::string passed() const;
    std::string left() const;
};

struct CompassInfo {
    Timestamp timestamp;
    float targetPitch;
    float targetYaw;
    float targetRoll;
    float targetAngle;
    int8_t hemisphere; // +1: front, -1: back, 0: invalid
    bool has_nav_target;
    dist_t nav_target_dist;
};

#endif //EDROBOT_TYPES_H

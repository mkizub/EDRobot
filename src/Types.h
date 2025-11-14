//
// Created by mkizub on 31.08.2025.
//

#ifndef EDROBOT_TYPES_H
#define EDROBOT_TYPES_H

typedef std::chrono::time_point<std::chrono::utc_clock> Timestamp;

enum class WState : int { Unknown=-1, Normal=0, Focused=1, Active=2, Disabled=3 };

enum class Lang { XX=-1, EN=0, RU=1 };

enum class GuiFocus { None=0, Right=1, Left=2, Chat=3, Role=4, Services=5, GalaxyMap=6, SystemMap=7, Orrery=8, FSS=9, SAA=10, Codex=11 };


// BodyType values game journals
//enum class JournalTypeBody {
//    Null, // a barycentre
//    Star,
//    Planet,
//    PlanetaryRing,
//    StellarRing,
//    Station,
//    AsteroidCluster,
//};
//
//enum class TypeBody {
//    Other, // barycenter
//    Star,
//    Planet,
//    Ring,
//    AsteroidCluster,
//};

enum class TypeNav {
    Other                   = 0,
    Error                   = 1,
    NotExplored             = 2,
    Signal                  = 3,
    WarZone                 = 4,
    ResSite                 = 5,
    StarSystem              = 6,
    Body                    = 0x10,   // generic type, also, Barycenter
    Barycenter              = 0x11,
    Ring                    = 0x12,
    AsteroidCluster         = 0x13,
    Star                    = 0x14,
    Planet                  = 0x15,
    SpaceThing              = 0x20,   // generic type
    NavBeacon               = 0x21,
    TouristBeacon           = 0x22,
    SpaceStation            = 0x27,   // generic type
    Orbis                   = 0x28,
    Ocellus                 = 0x29,
    Coriolis                = 0x2A,
    AsteroidBase            = 0x2B,
    SpaceOutpost            = 0x2C,
    SpaceInstallation       = 0x2D,
    SpaceConstrDepot        = 0x2F,
    Megaship                = 0x30,   // generic type
    StationMegaShip         = 0x31,
    FleetCarrier            = 0x32,
    SquadronCarrier         = 0x33,
    StrongholdCarrier       = 0x34,
    ColonisationShip        = 0x35,
    TrailblazerDream        = 0x36,
    PlanetaryThing          = 0x40,   // generic type
    PlanetaryStation        = 0x41,   // generic type
    PlanetaryPort           = 0x42,
    EngineerPort            = 0x43,
    Settlement              = 0x44,   // odyssey settlement
    PlanetaryInstallation   = 0x45,
    PlanetaryConstrDepot    = 0x46,
};

inline bool isSignal(TypeNav type) {
    return type < TypeNav::Body;
}
inline bool isBody(TypeNav type) {
    return type >= TypeNav::Body && type < TypeNav::SpaceThing;
}
inline bool isSite(TypeNav type) {
    return type >= TypeNav::SpaceThing;
}
inline bool isSpaceSite(TypeNav type) {
    return type >= TypeNav::SpaceThing && type < TypeNav::PlanetaryThing;
}
inline bool isSpaceStation(TypeNav type) {
    return type >= TypeNav::SpaceStation && type <= TypeNav::SpaceOutpost;
}
inline bool isPlanetarySite(TypeNav type) {
    return type >= TypeNav::PlanetaryThing && type <= TypeNav::PlanetaryConstrDepot;
}

namespace gal {
    class Entity;
    typedef std::shared_ptr<Entity> spEntity;
}

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

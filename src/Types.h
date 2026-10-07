//
// Created by mkizub on 31.08.2025.
//

#ifndef EDROBOT_TYPES_H
#define EDROBOT_TYPES_H

typedef std::chrono::time_point<std::chrono::utc_clock> Timestamp;

constexpr auto operator*(const Timestamp& tp) {
    return tp.time_since_epoch().count();
}
constexpr bool operator!(const Timestamp& tp) {
    return tp.time_since_epoch().count() == 0;
}

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

enum class TypeNav : uint8_t {
    Other                   = 0x0,
    Error                   = 0x1,
    NotExplored             = 0x2,
    StarSystem              = 0x3,
    Signal                  = 0x4,
    WarZone                 = 0x5,
    USS                     = 0x6,
    // stable interesting places
    ResSite                 = 0x11,
    NavBeacon               = 0x12,
    TouristBeacon           = 0x13,
    Codex                   = 0x14,  // notable stellar phenomena
    Titan                   = 0x15,
    // gap
    Body                    = 0x20,   // generic type
    Barycenter              = 0x21,
    Ring                    = 0x22,
    AsteroidCluster         = 0x23,
    Star                    = 0x24,
    Planet                  = 0x25,
    // gap
    SpaceStation            = 0x30,   // generic type
    Orbis                   = 0x31,
    Ocellus                 = 0x32,
    Dodec                   = 0x33,
    Coriolis                = 0x34,
    AsteroidBase            = 0x35,
    SpaceOutpost            = 0x36,
    // gap to add more stations (like Dodec was added)
    SpaceInstallation       = 0x3A,
    SpaceConstrDepot        = 0x3B,
    // gap
    Megaship                = 0x40,   // generic type
    StationMegaShip         = 0x41,
    FleetCarrier            = 0x42,
    SquadronCarrier         = 0x43,
    ColonisationShip        = 0x45,
    AnonMegaship            = 0x4A,   // all anonymouse megaships (Stronghold Carrier, Trailblazer Dream, Frontline Runner, etc)
    // gap
    PlanetaryThing          = 0x50,   // generic type
    PlanetaryPort           = 0x51,
    EngineerPort            = 0x52,   // "StationType":"CraterOutpost", "StationGovernment":"$government_Engineer;", "StationGovernment_Localised":"Мастерская"
    Settlement              = 0x53,   // odyssey settlement
    // gap
    PlanetaryInstallation   = 0x5A,
    PlanetaryConstrDepot    = 0x5B,

    // gap
    AnySite                 = 0x70,   // should be above SpaceStation, but put here to make both generic
};

inline constexpr bool isSignal(TypeNav type) {
    return type < TypeNav::Body;
}
inline constexpr bool isBody(TypeNav type) {
    return type >= TypeNav::Body && type < TypeNav::SpaceStation;
}
inline constexpr bool isAnySite(TypeNav type) {
    return type >= TypeNav::SpaceStation;   // should be above AnySite, but!
}
inline constexpr bool isGeneric(TypeNav type) {
    return (uint8_t(type) & 0xF) == 0;
}
inline constexpr bool isDatabaseSite(TypeNav type) {
    return type >= TypeNav::SpaceStation || (type == TypeNav::NavBeacon || type == TypeNav::Codex || type == TypeNav::Titan);
}
inline constexpr bool isSpaceSite(TypeNav type) {
    return type >= TypeNav::SpaceStation && type < TypeNav::PlanetaryThing;
}
inline constexpr bool isSpaceStation(TypeNav type) {
    return type >= TypeNav::SpaceStation && type <= TypeNav::SpaceOutpost;
}
inline constexpr bool isPlanetarySite(TypeNav type) {
    return type >= TypeNav::PlanetaryThing;
}
inline constexpr bool isConstrDepot(TypeNav type) {
    return type == TypeNav::SpaceConstrDepot ||
           type == TypeNav::PlanetaryConstrDepot ||
           type == TypeNav::ColonisationShip;
}
inline constexpr bool isCarrier(TypeNav type) {
    return type == TypeNav::FleetCarrier || type == TypeNav::SquadronCarrier;
}
inline constexpr bool isMegaship(TypeNav type) {
    return type >= TypeNav::Megaship && type <= TypeNav::AnonMegaship;
}
inline constexpr bool isAnonMegaship(TypeNav type) {
    return type == TypeNav::AnonMegaship;
}

namespace gal {
    class Entity;
    typedef std::shared_ptr<Entity> spEntity;
    class StarSystem;
    typedef std::shared_ptr<StarSystem> spStarSystem;

    struct TestBackdor;
}

namespace ai {

void toggleDebugPause();
bool isDebugPause();
void resetCompassDetects();

}

struct Coords {
    double x;
    double y;
    double z;

    constexpr bool empty() const {
        return (x*x + y*y + z*z) < 1.e-7;
    }
    constexpr bool operator==(const Coords& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        double dz = z - other.z;
        return (dx*dx + dy*dy + dz*dz) < 1.e-7;
    }
    constexpr bool operator!=(const Coords& other) const {
        return !operator==(other);
    }
};

inline double dist(const Coords& a, const Coords& b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) +
                     (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

struct opt_bool {
    int8_t val {-1};

    opt_bool() {}
    opt_bool(bool value) { val = value ? 1 : 0; }
    opt_bool(const opt_bool& value) { val = value.val; }

    constexpr opt_bool& operator=(const opt_bool& other) noexcept {
        val = other.val;
        return *this;
    }
    constexpr opt_bool& operator=(bool value) noexcept {
        val = value ? 1 : 0;
        return *this;
    }

    constexpr void reset() noexcept {
        val = -1;
    }

    void emplace() noexcept {
        val = -1;
    }

    bool emplace(bool value) noexcept {
        val = value ? 1 : 0;
        return val;
    }

    bool has_value() const {
        return val >= 0;
    }

    bool& value() {
        //if (val < 0) throw std::bad_optional_access();
        return reinterpret_cast<bool&>(val);
    }
    bool value() const {
        if (val < 0) throw std::bad_optional_access();
        return val > 0;
    }
    bool value_or(bool default_value = {}) const {
        if (val < 0) return default_value;
        return val;
    }
};

struct opt_float {
    float val;

    opt_float() : val {std::numeric_limits<float>::quiet_NaN()} {}
    opt_float(const opt_float& value) : val {value.val} {}
    opt_float(float value) : val {value} {}

    constexpr opt_float& operator=(const opt_float& other) noexcept {
        val = other.val;
        return *this;
    }
    constexpr opt_float& operator=(float value) noexcept {
        val = value;
        return *this;
    }

    constexpr void reset() noexcept {
        val = std::numeric_limits<float>::quiet_NaN();
    }

    float& emplace() noexcept {
        val = std::numeric_limits<float>::quiet_NaN();
        return val;
    }

    float& emplace(float value) noexcept {
        val = value;
        return val;
    }

    bool has_value() const {
        return !std::isnan(val);
    }

    float& value() & {
//        if (std::isnan(val)) throw std::bad_optional_access();
        return val;
    }
    const float& value() const & {
//        if (std::isnan(val)) throw std::bad_optional_access();
        return val;
    }
    float&& value() && {
//        if (std::isnan(val)) throw std::bad_optional_access();
        return std::move(val);
    }
    const float&& value() const && {
//        if (std::isnan(val)) throw std::bad_optional_access();
        return std::move(val);
    }
    float value_or(float&& default_value) && {
        if (std::isnan(val)) return default_value;
        return val;
    }
    float value_or(float&& default_value) const & {
        if (std::isnan(val)) return default_value;
        return val;
    }
};

struct body_id_t {
    short val;

    constexpr body_id_t() : val(-1) {}
    constexpr explicit body_id_t(int id) : val(id) {}
    constexpr body_id_t(const body_id_t& value) { val = value.val; }
    constexpr body_id_t(body_id_t&& value) { val = value.val; }

    constexpr body_id_t& operator=(const body_id_t& other) noexcept {
        val = other.val;
        return *this;
    }
    constexpr body_id_t& operator=(int value) noexcept {
        val = value;
        return *this;
    }

    auto operator==(const body_id_t& rhs) const {
        if (this->val < 0 || rhs.val < 0)
            return false;
        return this->val == rhs.val;
    }
    auto operator<=>(const body_id_t& rhs) const {
        if (this->val < 0 || rhs.val < 0)
            return std::partial_ordering::unordered;
        if (this->val < rhs.val)
            return std::partial_ordering::less;
        if (this->val > rhs.val)
            return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
    }

    constexpr void reset() noexcept {
        val = -1;
    }

    constexpr void emplace() noexcept {
        val = -1;
    }

    constexpr bool emplace(int value) noexcept {
        val = value;
        return val;
    }

    constexpr bool has_value() const {
        return val >= 0;
    }

    constexpr short& value() {
        //if (val < 0) throw std::bad_optional_access();
        return val;
    }
    constexpr short value() const {
        if (val < 0) throw std::bad_optional_access();
        return val > 0;
    }
};

struct dist_t {
    enum Unit {
        X, M, KM, MM, LS, LY
    };
    double dist;
    int tsec;
    Unit unit : 8;
    int8_t conf : 8;

    constexpr dist_t() : dist(0), tsec(0), unit(X), conf(0) {}
    constexpr dist_t(Unit u, double d) : unit(u), dist(d), tsec(0), conf(0) {}
    constexpr dist_t(double d, Unit u, int8_t conf) : dist(d), tsec(0), unit(u), conf(conf) {}
    constexpr dist_t(double d, int s, Unit u, int8_t conf) : dist(d), tsec(s), unit(u), conf(conf) {}

    explicit operator bool() const { return unit != X && dist > 0; }
    bool valid() const { return unit != X; }
    dist_t convertTo(Unit u) const;
    dist_t abs() const;
    double get(Unit u) const;
    double get_m() const;
    double get_km() const;
    double get_ls() const;
    std::string to_string() const;

    auto format_as(Unit u) -> std::string_view {
    }};
std::ostream& operator<<(std::ostream& os, const dist_t& obj);

template <>
struct std::formatter<dist_t::Unit> : std::formatter<std::string_view> {
    auto format(dist_t::Unit u, format_context& ctx) const {
        std::string_view sv;
        switch (u) {
        default:
        case dist_t::Unit::X: sv = ""; break;
        case dist_t::Unit::M: sv = "m"; break;
        case dist_t::Unit::KM: sv = "km"; break;
        case dist_t::Unit::MM: sv = "mm"; break;
        case dist_t::Unit::LS: sv = "ls"; break;
        case dist_t::Unit::LY: sv = "ly"; break;
        }
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};
template <>
struct std::formatter<dist_t> : std::formatter<std::string> {
    auto format(const dist_t& d, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}{}", d.dist, d.unit);
    }
};
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
    return {u, std::abs(d1.get(u) - d2.get(u))};
}
inline dist_t operator *(const dist_t d, double scale) {
    return {d.unit, d.dist * scale};
}
inline dist_t operator /(const dist_t d, double scale) {
    return {d.unit, d.dist / scale};
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

struct FovScale {
    FovScale() = default;
    FovScale(double fov0, double fov1, cv::Rect rect0, cv::Rect rect1);
    double fov54 = 54.32;
    double fov60 = 60.00;
    double scale60 = 0.888258;
    double getScaleForFOV(double fov);
    cv::Point apply(cv::Point p, double fov);
    cv::Size apply(cv::Size s, double fov);
    cv::Rect apply(cv::Rect r, double fov);
    cv::Line apply(cv::Line l, double fov);
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

struct GameKey {
    enum Device { Void, Keyboard, Mouse, vJoy };
    Device device {Void};
    std::string key;
    int code {0}; // scancode or mouse button
    std::vector<GameKey> modifiers;
    friend std::ostream& operator<<(std::ostream& os, const GameKey& obj);
};
struct KeyBindings {
    enum Mode { Hold, Toggle, Axis, AxisInv };
    std::string action;
    Mode mode {Hold};
    GameKey primary;
    GameKey secondary;
};

struct Axis {
    enum Type { Pitch, Yaw, Roll };
    Axis(Type type) : type(type) {}

    const Type type;
    KeyBindings bindings;
    double value;
    Timestamp start;
    Timestamp stop;

    static void resetAll(bool reset_mouse=false);
    const std::string_view name() { return enum_name<Axis::Type>(type); }
    void set(double val, int duration);
    void setRaw(double val, int duration);
    void reset();
    [[nodiscard]] bool active() const;

    double timeScaleFor(double val) const;
    double valueScaleFor(double val) const;
};

extern Axis pitchAxis;
extern Axis yawAxis;
extern Axis rollAxis;

class CommodityCategory {
    friend class Configuration;
public:
    int intId; // from market filter
    std::string nameId;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::array<std::string,2> translation;
};

struct MarketLine {
    int buyPrice;
    int sellPrice;
    int stock;
    int demand;
    opt_bool isConsumer;
    opt_bool isProducer;
};

struct Commodity {
    friend class Configuration;
public:
    int intId;
    std::string nameId;
    CommodityCategory* category;
    std::string name;   // current localization
    std::wstring wide;  // same as 'name'
    std::wstring wocr;  // same as 'wide' but with OCR chars
    std::array<std::string,2> translation;
    int carrierSortingOrder[2];

    bool rare;

    struct {
        int count;
        int stolen;
    } ship;

    struct {
        int count;
    } fc;
};

struct RavenProj {
    struct CmdrInfo {
        Timestamp timestamp {}; // contribution timestamp
        int deliveries {}; // number of deliveries
        int contributed {}; // total cargo contributed
        std::vector<Commodity*> assigned; // commodities assigned to this commander
    };
    std::string buildId;
    std::string status;
    Timestamp timestamp {}; // project timestamp
    std::map<std::string,CmdrInfo> commanders;
};

typedef std::shared_ptr<RavenProj> spRavenProj;

struct Market {
    const Timestamp timestamp;
    const int64_t marketId;
    const int64_t systemAddress;
    const std::string stationName;
    const std::string stationType;
    const std::string starSystem;
    spRavenProj raven;
    std::unordered_map<Commodity*,MarketLine> items;

    std::string_view ravenBuildId() {
        if (!raven)
            return {};
        return raven->buildId;
    }
};

struct ShipCargo {
    const std::vector<Commodity*> cargo;
    const Timestamp timestamp;
    const int count {0};
    const std::string vessel;
};

struct NavRoute {
    struct Entry {
        std::string starSystem;
        int64_t systemAddress;
        cv::Point3d starpos;
        std::string starClass;
    };
    const Timestamp timestamp;
    const std::vector<Entry> route;
};

struct GameEvent {
    const js::value data;
    const Timestamp timestamp;
    const std::string event;
    const bool expired;
};

typedef std::shared_ptr<Market> spMarket;
typedef std::shared_ptr<ShipCargo> spShipCargo;
typedef std::shared_ptr<NavRoute> spNavRoute;
typedef std::shared_ptr<GameEvent> spGameEvent;

class GameEventQueue {
    std::deque<spGameEvent> events;
    std::mutex mutex;
    std::condition_variable condvar;
    int seconds_to_keep;
    std::deque<spGameEvent>::iterator find_event_locked(const std::initializer_list<std::string_view>& lst);
public:
    GameEventQueue(int seconds_to_keep) : seconds_to_keep(seconds_to_keep) {}
    void clear();
    void clear(std::chrono::milliseconds seconds_expired);
    void push(spGameEvent& ge);
    spGameEvent pop(std::chrono::milliseconds timeout);
    spGameEvent wait_event(std::chrono::milliseconds timeout, bool pop, std::initializer_list<std::string_view> ids);
};

struct Bookmark {
    const std::string name;
    const std::string system;
    const std::string dock;
    const std::string comment;
};
typedef std::shared_ptr<Bookmark> spBookmark;


struct LandingPads {
    uint8_t large;
    uint8_t medium;
    uint8_t small;
    [[nodiscard]] bool empty() const { return large==0 && medium==0 && small==0; }
    [[nodiscard]] bool has_value() const { return large==0 && medium==0 && small==0; }
};

#define JS_ENUM_DECL(NAME) struct NAME : public js::Enum<NAME> {};

JS_ENUM_DECL(JsAllegiance)
JS_ENUM_DECL(JsGovernment)
JS_ENUM_DECL(JsEconomy)
JS_ENUM_DECL(JsSecurity)
JS_ENUM_DECL(JsFactionState)
JS_ENUM_DECL(JsPower)
JS_ENUM_DECL(JsPowerState)
JS_ENUM_DECL(JsThargoidState)
JS_ENUM_DECL(JsNavType)
JS_ENUM_DECL(JsBodySubType)
JS_ENUM_DECL(JsVolcanismType)
JS_ENUM_DECL(JsAtmosphereType)
JS_ENUM_DECL(JsSolidType)
JS_ENUM_DECL(JsTerraformingState)
JS_ENUM_DECL(JsMaterials)
JS_ENUM_DECL(JsReserveLevel)
JS_ENUM_DECL(JsTimestamps)
JS_ENUM_DECL(JsServices)
JS_ENUM_DECL(JsStationState)
JS_ENUM_DECL(JsCarrierDockingAccess)
JS_ENUM_DECL(JsLuminosity)

inline TypeNav toTypeNav(JsNavType tp) { return static_cast<TypeNav>(tp.id); }
inline JsNavType toJsNavType(TypeNav tp) { return JsNavType::get(std::to_underlying(tp)); }


#endif //EDROBOT_TYPES_H

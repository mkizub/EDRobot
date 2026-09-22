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

enum class TypeNav : uint8_t {
    Other                   = 0,
    Error                   = 1,
    NotExplored             = 2,
    Signal                  = 3,
    WarZone                 = 4,
    ResSite                 = 5,
    StarSystem              = 6,
    Body                    = 0x10,   // generic type
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
    Dodec                   = 0x2A,
    Coriolis                = 0x2B,
    AsteroidBase            = 0x2C,
    SpaceOutpost            = 0x2D,
    SpaceInstallation       = 0x2E,
    SpaceConstrDepot        = 0x2F,
    Megaship                = 0x30,   // generic type
    StationMegaShip         = 0x31,
    FleetCarrier            = 0x32,
    SquadronCarrier         = 0x33,
    StrongholdCarrier       = 0x34,
    ColonisationShip        = 0x35,
    PlanetaryThing          = 0x40,   // generic type
    PlanetaryPort           = 0x42,
    EngineerPort            = 0x43,
    Settlement              = 0x44,   // odyssey settlement
    PlanetaryInstallation   = 0x45,
    PlanetaryConstrDepot    = 0x46,
};

inline constexpr bool isSignal(TypeNav type) {
    return type < TypeNav::Body;
}
inline constexpr bool isBody(TypeNav type) {
    return type >= TypeNav::Body && type < TypeNav::SpaceThing; // or (std::to_underlying(type) & 0xF0) == 0x10;
}
inline constexpr bool isSite(TypeNav type) {
    return type >= TypeNav::SpaceThing;
}
inline constexpr bool isSpaceSite(TypeNav type) {
    return type >= TypeNav::SpaceThing && type < TypeNav::PlanetaryThing;
}
inline constexpr bool isSpaceStation(TypeNav type) {
    return type >= TypeNav::SpaceStation && type <= TypeNav::SpaceOutpost;
}
inline constexpr bool isPlanetarySite(TypeNav type) {
    return type >= TypeNav::PlanetaryThing; /*&& type <= TypeNav::PlanetaryConstrDepot;*/ // or (std::to_underlying(type) & 0xF0) == 0x40;
}
inline constexpr bool isConstrDepot(TypeNav type) {
    return type == TypeNav::SpaceConstrDepot ||
           type == TypeNav::PlanetaryConstrDepot ||
           type == TypeNav::ColonisationShip;
}

namespace gal {
    class Entity;
    typedef std::shared_ptr<Entity> spEntity;
    class StarSystem;
    typedef std::shared_ptr<StarSystem> spStarSystem;
}

namespace ai {

void toggleDebugPause();
bool isDebugPause();
void resetCompassDetects();

}

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
    int meanPrice;
    int stock;
    int demand;
    uint8_t stockBracket;
    uint8_t demandBracket;
    bool isConsumer;
    bool isProducer;
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
    bool value_or(bool default_value) const {
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

struct LandingPads {
    int8_t large {};
    int8_t medium {};
    int8_t small {};
    [[nodiscard]] bool empty() const { return large==0 && medium==0 && small==0; }
    [[nodiscard]] bool has_value() const { return large==0 && medium==0 && small==0; }
};

#define JS_ENUM_DECL(NAME)                                                      \
    struct Js##NAME##Decl : public js::EnumDecl<struct Js##NAME> {};            \
    struct Js##NAME : public js::Enum<Js##NAME##Decl> {};

JS_ENUM_DECL(Allegiance)
JS_ENUM_DECL(Government)
JS_ENUM_DECL(Economy)
JS_ENUM_DECL(Security)
JS_ENUM_DECL(FactionState)
JS_ENUM_DECL(Power)
JS_ENUM_DECL(PowerState)
JS_ENUM_DECL(ThargoidState)
JS_ENUM_DECL(BodyType)
JS_ENUM_DECL(BodySubType)
JS_ENUM_DECL(VolcanismType)
JS_ENUM_DECL(AtmosphereType)
JS_ENUM_DECL(SolidType)
JS_ENUM_DECL(TerraformingState)
JS_ENUM_DECL(Materials)
JS_ENUM_DECL(ReserveLevel)
JS_ENUM_DECL(Timestamps)
JS_ENUM_DECL(Services)
JS_ENUM_DECL(StationType)
JS_ENUM_DECL(StationState)
JS_ENUM_DECL(CarrierDockingAccess)
JS_ENUM_DECL(SpectralClass)
JS_ENUM_DECL(Luminosity)

inline TypeNav toTypeNav(JsBodyType tp) { return static_cast<TypeNav>(tp.id); }
inline TypeNav toTypeNav(JsStationType tp) { return static_cast<TypeNav>(tp.id); }
inline JsBodyType toJsBodyType(TypeNav tp) { return JsBodyType::get(std::to_underlying(tp)); }
inline JsStationType toJsStationType(TypeNav tp) { return JsStationType::get(std::to_underlying(tp)); }


#endif //EDROBOT_TYPES_H

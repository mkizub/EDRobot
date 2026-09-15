//
// Created by mkizub on 09.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include "JsEnum.h"
#include <glaze/cbor.hpp>

#define MSGPACK_NO_BOOST 1
#include <msgpack.hpp>

const int FMT = glz::CBOR;

namespace glz
{

template <JsEnumDeclType E>
struct from<FMT, db::JsEnum<E>>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        uint8_t type = *reinterpret_cast<uint8_t*>(it);
        uint8_t major_type = glz::cbor::get_major_type(type);
        if (major_type == glz::cbor::major::tstr) {
            std::string str;
            parse<FMT>::op<Opts>(str, ctx, it, end);
            value.ptr = E::instance.get(str);
            if (!value.ptr)
                value.ptr = E::instance.addNewValue(str);
        } else {
            unsigned id{};
            parse<FMT>::op<Opts>(id, ctx, it, end);
            value.ptr = E::instance.get(id);
        }
    }
};

template <JsEnumDeclType E>
struct to<FMT, db::JsEnum<E>>
{
    template <auto Opts>
    static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
    {
        if (!value.ptr)
            serialize<JSON>::op<Opts>(nullptr, ctx, b, ix);
        else if (!value.ptr->id && !value.ptr->str.empty())
            serialize<JSON>::op<Opts>(value.ptr->str, ctx, b, ix);
        else
            serialize<JSON>::op<Opts>(value.ptr->id, ctx, b, ix);
    }
};

template <>
struct from<FMT, Timestamp>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        glz::epoch_millis tp;
        parse<FMT>::op<Opts>(tp, ctx, it, end);
        value = Timestamp(std::chrono::duration_cast<Timestamp::duration>(tp.value.time_since_epoch()));
    }
};

template <>
struct to<FMT, Timestamp>
{
    template <auto Opts>
    static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
    {
        std::chrono::system_clock::time_point tp(std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()));
        serialize<FMT>::op<Opts>(glz::epoch_millis(tp), ctx, b, ix);
    }
};

} // namespace glz

#define F_KEY(id) "f-key-" id
#define S_KEY(id) "s-key-" id
#define B_KEY(id) "b-key-" id
#define SS_KEY(id) "ss-key-" id

template <>
struct glz::meta<db::Faction>
{
    using T = db::Faction;
    static constexpr auto value = glz::object(
            F_KEY("1"), &T::id,
            F_KEY("2"), &T::name,
            F_KEY("3"), &T::allegiance,
            F_KEY("4"), &T::government
    );
};

template <>
struct glz::meta<db::StationJS>
{
    using T = db::StationJS;

    static constexpr auto value = glz::object(
            S_KEY("1"), &T::id,
            S_KEY("2"), &T::type,
            S_KEY("3"), &T::name,
            S_KEY("4"), &T::updated_at,
            S_KEY("5"), &T::realName,
            S_KEY("6"), &T::carrierName,
            S_KEY("7"), &T::controllingFaction,
            S_KEY("8"), &T::controllingFactionState,
            S_KEY("9"), &T::distanceToArrival,
            S_KEY("A"), &T::primaryEconomy,
            S_KEY("B"), &T::secondaryEconomy,
            S_KEY("C"), &T::economies,
            S_KEY("D"), &T::allegiance,
            S_KEY("E"), &T::government,
            S_KEY("F"), &T::services,
            S_KEY("G"), &T::state,
            S_KEY("I"), &T::latitude,
            S_KEY("H"), &T::longitude,
            S_KEY("J"), &T::landingPads,
            S_KEY("K"), &T::carrierDockingAccess,
            "market", glz::skip(),
            "shipyard", glz::skip(),
            "outfitting", glz::skip()
    );
};

template <>
struct glz::meta<db::BodyJS>
{
    using T = db::BodyJS;

    static constexpr auto value  = glz::object(
            B_KEY("1"), &T::type,
            B_KEY("2"), &T::bodyId,
            B_KEY("3"), &T::name,
            B_KEY("4"), &T::subType,
            B_KEY("5"), &T::parents,
            B_KEY("6"), &T::orbitalPeriod,
            B_KEY("7"), &T::semiMajorAxis,
            B_KEY("8"), &T::orbitalEccentricity,
            B_KEY("9"), &T::orbitalInclination,
            B_KEY("A"), &T::argOfPeriapsis,
            B_KEY("B"), &T::meanAnomaly,
            B_KEY("C"), &T::ascendingNode,
            B_KEY("D"), &T::distanceToArrival,
            B_KEY("E"), &T::surfaceTemperature,
            B_KEY("F"), &T::rotationalPeriod,
            B_KEY("G"), &T::axialTilt,
            B_KEY("H"), &T::updated_at,
            B_KEY("I"), &T::timestamps,
            B_KEY("J"), &T::rotationalPeriodTidallyLocked,
            B_KEY("K"), &T::stations,
            "belts", glz::skip(),
            "rings", glz::skip(),
            "signals", glz::skip(),

            // star part
            B_KEY("a"), glz::custom<&T::set_mainStar, &T::get_mainStar>,
            B_KEY("b"), glz::custom<&T::set_age, &T::get_age>,
            B_KEY("c"), glz::custom<&T::set_spectralClass, &T::get_spectralClass>,
            B_KEY("d"), glz::custom<&T::set_luminosity, &T::get_luminosity>,
            B_KEY("e"), glz::custom<&T::set_absoluteMagnitude, &T::get_absoluteMagnitude>,
            B_KEY("f"), glz::custom<&T::set_solarMasses, &T::get_solarMasses>,
            B_KEY("g"), glz::custom<&T::set_solarRadius, &T::get_solarRadius>,

            // plant part
            B_KEY("k"), glz::custom<&T::set_isLandable, &T::get_isLandable>,
            B_KEY("l"), glz::custom<&T::set_gravity, &T::get_gravity>,
            B_KEY("m"), glz::custom<&T::set_earthMasses, &T::get_earthMasses>,
            B_KEY("n"), glz::custom<&T::set_radius, &T::get_solarRadius>,
            B_KEY("o"), glz::custom<&T::set_surfacePressure, &T::get_surfacePressure>,
            B_KEY("p"), glz::custom<&T::set_volcanismType, &T::get_volcanismType>,
            B_KEY("q"), glz::custom<&T::set_atmosphereType, &T::get_atmosphereType>,
            B_KEY("r"), glz::custom<&T::set_atmosphereComposition, &T::get_atmosphereComposition>,
            B_KEY("s"), glz::custom<&T::set_solidComposition, &T::get_solidComposition>,
            B_KEY("t"), glz::custom<&T::set_materials, &T::get_materials>,
            B_KEY("u"), glz::custom<&T::set_terraformingState, &T::get_terraformingState>,
            B_KEY("v"), glz::custom<&T::set_reserveLevel, &T::get_reserveLevel>
    );
};

template <>
struct glz::meta<db::StarSystemJS>
{
    using T = db::StarSystemJS;

    static constexpr auto value = glz::object(
            SS_KEY("1"), &T::id64,
            SS_KEY("2"), &T::name,
            SS_KEY("3"), &T::coords,
            SS_KEY("4"), &T::allegiance,
            SS_KEY("5"), &T::government,
            SS_KEY("6"), &T::primaryEconomy,
            SS_KEY("7"), &T::secondaryEconomy,
            SS_KEY("8"), &T::security,
            SS_KEY("9"), &T::population,
            SS_KEY("A"), &T::bodyCount,
            SS_KEY("B"), &T::updated_at,
            SS_KEY("C"), &T::controllingFaction,
            SS_KEY("D"), &T::factions,
            SS_KEY("E"), &T::powerState,
            SS_KEY("F"), &T::powerConflictProgress,
            SS_KEY("G"), &T::powers,
            SS_KEY("H"), &T::controllingPower,
            SS_KEY("I"), &T::powerStateControlProgress,
            SS_KEY("J"), &T::powerStateReinforcement,
            SS_KEY("K"), &T::powerStateUndermining,
            SS_KEY("L"), &T::thargoidWar,
            SS_KEY("M"), &T::bodies,
            SS_KEY("N"), &T::stations
    );
};


thread_local std::string tlStarSystemName;

namespace msgpack {
namespace v3 {
namespace adaptor {

template <typename Stream>
inline msgpack::packer<Stream>& pack_key(const char* nm, msgpack::packer<Stream>& m, uint32_t& key_count, uint32_t k) {
    key_count += 1;
    m.pack_unsigned_int(k);
    //auto s = std::format("{}-key-{}", nm, k);
    //m.pack_str(s.size()).pack_str_body(s.data(), s.size());
    return m;
}

template <typename T>
inline bool need_value(const T& val) {
    if constexpr (std::same_as<T, Timestamp>) {
        return val.time_since_epoch().count() != 0;
    }
    else if constexpr (std::same_as<T, bool>) {
        return val;
    }
    else if constexpr (std::is_floating_point_v<T>) {
        return !std::isnan(val);
    }
    else if constexpr (std::is_integral_v<T>) {
        return val != 0;
    }
    else if constexpr (std::same_as<std::string, T>) {
        return !val.empty();
    }
    else if constexpr (std::same_as<std::string, T>) {
        return !val.empty();
    }
    else if constexpr (std::same_as<db::LandingPadsJS, T>) {
        return !val.empty();
    }
    return true;
}

template <typename K, typename V>
inline bool need_value(const ed::small_map<K,V>& v) {
    return !v.empty();
}

template <typename V>
inline bool need_value(const std::vector<V>& v) {
    return !v.empty();
}

template <typename V>
inline bool need_value(const std::unique_ptr<V>& v) {
    return v.get() != nullptr;
}

template <typename E>
inline bool need_value(const db::JsEnum<E>& v) {
    return bool(v);
}



#define MAP_START(o)    {   uint32_t key_count = 0;     \
                            msgpack::sbuffer mb;        \
                            msgpack::packer<msgpack::sbuffer> m(mb);

#define MAP_END(o)          o.pack_map(key_count);      \
                            o.pack_bin_body(mb.data(),mb.size());   }

#define PK(nm, k)           pack_key(nm, m, key_count, k)

#define PK_IF(nm, k, v)     if (need_value(v)) pack_key(nm, m, key_count, k).pack(v)


template<>
struct pack<bool> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const bool& v) const {
        if (v)
            o.pack_true();
        else
            o.pack_false();
        return o;
    }
};

template<>
struct pack<float> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const float& v) const {
        if (v == 0)
            o.pack_unsigned_int(0);
        else
            o.pack_float(v);
        return o;
    }
};

template<>
struct pack<double> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const double& v) const {
        if (v == 0)
            o.pack_unsigned_int(0);
        else
            o.pack_double(v);
        return o;
    }
};

template<>
struct pack<int64_t> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const int64_t& v) const {
        o.pack_long_long(v);
        return o;
    }
};

template<>
struct pack<uint64_t> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const uint64_t& v) const {
        o.pack_unsigned_long_long(v);
        return o;
    }
};

template<>
struct pack<Timestamp> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const Timestamp& v) const {
        if (v.time_since_epoch().count() == 0)
            o.pack_nil();
        else if ((v.time_since_epoch().count() % Timestamp::period::den) == 0)
            o.pack_fix_uint32(v.time_since_epoch().count() / Timestamp::period::den);
        else
            o.pack_fix_int64(v.time_since_epoch().count());
        return o;
    }
};

template<>
struct pack<std::string> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::string& v) const {
        o.pack_str(v.size()).pack_str_body(v.data(), v.size());
        return o;
    }
};

template<>
struct pack<std::string_view> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::string_view& v) const {
        o.pack_str(v.size()).pack_str_body(v.data(), v.size());
        return o;
    }
};

template<typename T>
struct pack<std::unique_ptr<T>> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::unique_ptr<T>& v) const {
        if (v.get() == nullptr)
            o.pack_nil();
        else
            o.pack(*v.get());
        return o;
    }
};

template<typename ES>
struct pack<db::JsEnum<ES>> {
    using T = db::JsEnum<ES>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        if (!v.ptr)
            o.pack_nil();
        else if (!v.ptr->id && !v.ptr->str.empty())
            o.pack_str(v.ptr->str.size()).pack_str_body(v.ptr->str.data(), v.ptr->str.size());
        else
            o.pack_unsigned_int(v.ptr->id);
        return o;
    }
};


template<typename E>
struct pack<std::vector<E>> {
    using T = std::vector<E>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(v.size());
        for (int i=0; i < v.size(); i++)
            o.pack(v[i]);
        return o;
    }
};

template<>
struct pack<std::vector<db::BodyParentJS>> {
    using T = std::vector<db::BodyParentJS>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(v.size()*2);
        for (int i=0; i < v.size(); i++) {
            o.pack(v[i].type);
            o.pack_unsigned_int(v[i].bodyId);
        }
        return o;
    }
};

template<typename E, typename V>
struct pack<ed::small_map<db::JsEnum<E>,V>> {
    using T = ed::small_map<db::JsEnum<E>,V>;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_map(v.size());
        for (auto& [key,val] : v) {
            o.pack(key);
            if constexpr (std::same_as<V, Timestamp>) {
                return o.pack_fix_int64(val.time_since_epoch().count());
            }
            else if constexpr (std::same_as<V, bool>) {
                if (val)
                    o.pack_true();
                else
                    o.pack_false();
            }
            else if constexpr (std::same_as<V, double>) {
                o.pack_double(val);
            }
            else if constexpr (std::same_as<V, float>) {
                o.pack_float(val);
            }
            else if constexpr (std::is_integral_v<V>) {
                o.pack_int(val);
            }
            else {
                o.pack(val);
            }
        }
        return o;
    }
};


template<>
struct pack<db::PowerConflictJS> {
    using T = db::PowerConflictJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack(v.power);
        o.pack_double(v.progress);
        return o;
    }
};

template<>
struct pack<db::FactionStateJS> {
    using T = db::FactionStateJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack(v.state);
        if (!std::isnan(v.trend))
            o.pack_float(v.trend);
        else
            o.pack_nil();
        return o;
    }
};

template<>
struct pack<db::FactionJS> {
    using T = db::FactionJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack(v.name);
        MAP_START(o)
            PK_IF("fa", 1, v.state);
            PK_IF("fa", 2, v.allegiance);
            PK_IF("fa", 3, v.government);
            PK_IF("fa", 4, v.influence);
            PK_IF("fa", 5, v.activeStates);
            PK_IF("fa", 6, v.pendingStates);
            PK_IF("fa", 7, v.recoveringStates);
        MAP_END(o)
        return o;
    }
};

template<>
struct pack<db::ThargoidWarJS> {
    using T = db::ThargoidWarJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        MAP_START(o)
            PK_IF("tw", 1, v.currentState);
            PK_IF("tw", 2, v.successState);
            PK_IF("tw", 3, v.failureState);
            PK_IF("tw", 4, v.progress);
            PK_IF("tw", 5, v.daysRemaining);
            PK_IF("tw", 6, v.portsRemaining);
            PK_IF("tw", 7, v.successReached);
        MAP_END(o)
        return o;
    }
};

template<>
struct pack<db::LandingPadsJS> {
    using T = db::LandingPadsJS;
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const T& v) const {
        o.pack_array(3);
        o.pack_unsigned_int(v.large);
        o.pack_unsigned_int(v.medium);
        o.pack_unsigned_int(v.small);
        return o;
    }
};

template<>
struct pack<db::StationJS> {

    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, db::StationJS const& v) const {
        o.pack_fix_int64(v.id);
        o.pack(v.type);
        o.pack(v.name);
        MAP_START(o)
            PK_IF("st", 1, v.updated_at);
            PK_IF("st", 2, v.realName);
            PK_IF("st", 3, v.carrierName);
            PK_IF("st", 4, v.controllingFaction);
            PK_IF("st", 5, v.controllingFactionState);
            PK_IF("st", 6, v.primaryEconomy);
            PK_IF("st", 7, v.secondaryEconomy);
            PK_IF("st", 8, v.economies);
            PK_IF("st", 9, v.allegiance);
            PK_IF("st", 10, v.government);
            PK_IF("st", 11, v.state);
            PK_IF("st", 12, v.distanceToArrival);
            PK_IF("st", 13, v.latitude);
            PK_IF("st", 14, v.latitude);
            PK_IF("st", 15, v.landingPads);
            PK_IF("st", 16, v.carrierDockingAccess);
            //PK_IF("st", 17, v.services);
        MAP_END(o)
        return o;
    }
};

template<>
struct pack<db::BodyJS> {

    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, db::BodyJS const& v) const {
        if (!v.type) {
            o.pack_nil();
            return o;
        }
        o.pack(v.type);
        o.pack_int(v.bodyId);
        bool is_star = v.type == db::JsBodyType::instance.get("Star");
        bool is_planet = v.type == db::JsBodyType::instance.get("Planet");
        bool is_cluster = v.type == db::JsBodyType::instance.get("Asteroid Cluster");
        MAP_START(o)
            if ((is_star || is_planet || is_cluster) && !v.name.empty()) {
                std::string v_name = v.name;
                auto &sn = tlStarSystemName;
                if (!sn.empty()) {
                    if (is_star && v_name == sn)
                        v_name = "";
                    else if (v_name.size() >= sn.size() + 2 && v_name.starts_with(sn) && v_name[sn.size()] == ' ')
                        v_name = v_name.substr(sn.size());
                    if (is_cluster) {
                        auto p = v_name.find("Belt Cluster");
                        if (p != std::string::npos)
                            v_name = v_name.replace(p, 12, "$");
                    }
                }
                //if (v_name == v.name)
                //    LOG_INFO("Special body name: {}", v_name);
                PK("bo", 1).pack(v_name);
            }
            PK_IF("bo", 2, v.subType);
            PK_IF("bo", 3, v.updated_at);
            PK_IF("bo", 4, v.orbitalPeriod);
            PK_IF("bo", 5, v.semiMajorAxis);
            PK_IF("bo", 6, v.orbitalEccentricity);
            PK_IF("bo", 7, v.orbitalInclination);
            PK_IF("bo", 8, v.argOfPeriapsis);
            PK_IF("bo", 9, v.meanAnomaly);
            PK_IF("bo", 10, v.ascendingNode);
            PK_IF("bo", 11, v.distanceToArrival);
            PK_IF("bo", 12, v.surfaceTemperature);
            PK_IF("bo", 13, v.rotationalPeriod);
            PK_IF("bo", 14, v.axialTilt);
            PK_IF("bo", 15, v.rotationalPeriodTidallyLocked);
            PK_IF("bo", 16, v.timestamps);
            PK_IF("bo", 17, v.parents);
            PK_IF("bo", 18, v.stations);
            // "rings"
            // "belts"
            if (auto* st=v.getStarPart(); st && is_star) {
                PK_IF("bs", 30, st->mainStar);
                PK_IF("bs", 31, st->age);
                PK_IF("bs", 32, st->spectralClass);
                PK_IF("bs", 33, st->luminosity);
                PK_IF("bs", 34, st->absoluteMagnitude);
                PK_IF("bs", 35, st->solarMasses);
                PK_IF("bs", 36, st->solarRadius);
            }
            else if (auto* pl=v.getPlanetPart(); pl && is_planet) {
                PK_IF("bp", 50, pl->isLandable);
                PK_IF("bp", 51, pl->gravity);
                PK_IF("bp", 52, pl->earthMasses);
                PK_IF("bp", 53, pl->radius);
                PK_IF("bp", 54, pl->terraformingState);
                PK_IF("bp", 55, pl->reserveLevel);
                PK_IF("bp", 56, pl->surfacePressure);
                PK_IF("bp", 57, pl->volcanismType);
                PK_IF("bp", 58, pl->atmosphereType);
                //PK_IF("bp", 59, pl->atmosphereComposition);
                //PK_IF("bp", 60, pl->solidComposition);
                //PK_IF("bp", 61, pl->materials);
                // "signals"
            }
        MAP_END(o)
        return o;
    }
};

template<>
struct pack<db::StarSystemJS> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, db::StarSystemJS const& v) const {
        o.pack_fix_int64(v.id64);
        o.pack(v.name);
        o.pack_double(v.coords.x);
        o.pack_double(v.coords.y);
        o.pack_double(v.coords.z);
        MAP_START(o)
            PK_IF("ss", 1, v.allegiance);
            PK_IF("ss", 2, v.government);
            PK_IF("ss", 3, v.primaryEconomy);
            PK_IF("ss", 4, v.secondaryEconomy);
            PK_IF("ss", 5, v.security);
            PK_IF("ss", 6, v.population);
            PK_IF("ss", 7, v.bodyCount);
            PK_IF("ss", 8, v.updated_at);
            PK_IF("ss", 9, v.controllingFaction);
            PK_IF("ss", 10, v.bodies);
            PK_IF("ss", 11, v.stations);
            //PK_IF("ss", 12, v.factions);
            //PK_IF("ss", 13, v.powerState);
            //PK_IF("ss", 14, v.powerConflictProgress);
            //PK_IF("ss", 15, v.powers);
            //PK_IF("ss", 16, v.controllingPower);
            //PK_IF("ss", 17, v.powerStateControlProgress);
            //PK_IF("ss", 18, v.powerStateReinforcement);
            //PK_IF("ss", 19, v.powerStateUndermining);
            //PK_IF("ss", 20, v.thargoidWar);
        MAP_END(o)
        return o;
    }
};

//template<>
//struct convert<db::StarSystemJS> {
//    msgpack::object const& operator()(msgpack::object const& o, db::StarSystemJS& v) const {
//        v.id64 = o.as<int64_t>();
////        // Ensure the incoming object is actually an array with expected elements
////        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
////        if (o.via.array.size < 2) { throw msgpack::type_error(); }
////
////        // Manually map array elements back to class fields
////        v.name = o.via.array.ptr[0].as<std::string>();
////        v.age  = o.via.array.ptr[1].as<int>();
//
//        return o;
//    }
//};

} // namespace adaptor
} // namespace MSGPACK_DEFAULT_API_NS
} // namespace msgpack


namespace db {

std::ofstream dbg_cbor;

void test_cbor_start() {
    dbg_cbor.open("cache/dbg.cbor", std::ios::out|std::ios::trunc|std::ios::binary);
}
void test_cbor_end() {
    dbg_cbor.close();
}

int test_cbor(StarSystemJS& ss_js) {
    tlStarSystemName = ss_js.name;
    try {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, ss_js);

        if (dbg_cbor.is_open())
            dbg_cbor.write(buffer.data(), buffer.size());

        return (int)buffer.size();
    } catch (const std::exception& e) {
        LOG_ERROR("Packing error");
        return 0;
    }
}

}
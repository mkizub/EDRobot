//
// Created by mkizub on 09.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include "JsEnum.h"
#include <glaze/cbor.hpp>

const int FMT = glz::CBOR;

namespace glz
{

template <JsEnumDeclType E>
struct from<FMT, db::JsEnum<E>>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        unsigned id{};
        parse<FMT>::op<Opts>(id, ctx, it, end);
        value.ptr = E::instance.get(id);
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


template <>
struct glz::meta<db::Faction>
{
    using T = db::Faction;
    static constexpr auto value = glz::object(
            "1", &T::id,
            "2", &T::name,
            "3", &T::allegiance,
            "4", &T::government
    );
};

template <>
struct glz::meta<db::BodyJS>
{
    using T = db::BodyJS;

    template <class T>
    static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&) {
        using V = std::decay_t<T>;
        if constexpr (std::same_as<V, Timestamp>) {
            return value.time_since_epoch().count();
        }
        if constexpr (std::same_as<T, bool>) {
            return value == false;
        }
        if constexpr (std::is_floating_point_v<T>) {
            return value.time_since_epoch().count();
        }
        if constexpr (std::is_integral_v<T>) {
            return value == 0;
        }
        if constexpr (std::same_as<T, std::string>) {
            return value.empty();
        }
        return false;
    }

    static constexpr auto value  = glz::object(
            "1", &T::type,
            "2", &T::bodyId,
            "3", &T::name,
            "4", &T::subType,
            "5", &T::parents,
            "6", &T::orbitalPeriod,
            "7", &T::semiMajorAxis,
            "8", &T::orbitalEccentricity,
            "9", &T::orbitalInclination,
            "10", &T::argOfPeriapsis,
            "11", &T::meanAnomaly,
            "12", &T::ascendingNode,
            "13", &T::distanceToArrival,
            "14", &T::surfaceTemperature,
            "15", &T::rotationalPeriod,
            "16", &T::axialTilt,
            "17", &T::updated_at,
            "18", &T::timestamps,
            "19", &T::rotationalPeriodTidallyLocked,
            "belts", glz::skip(),
            "rings", glz::skip(),
            "signals", glz::skip(),
            "stations", glz::skip(),

            // star part
            "30", glz::custom<&T::set_mainStar, &T::get_mainStar>,
            "31", glz::custom<&T::set_age, &T::get_age>,
            "32", glz::custom<&T::set_spectralClass, &T::get_spectralClass>,
            "33", glz::custom<&T::set_luminosity, &T::get_luminosity>,
            "34", glz::custom<&T::set_absoluteMagnitude, &T::get_absoluteMagnitude>,
            "35", glz::custom<&T::set_solarMasses, &T::get_solarMasses>,
            "36", glz::custom<&T::set_solarRadius, &T::get_solarRadius>,

            // plant part
            "40", glz::custom<&T::set_isLandable, &T::get_isLandable>,
            "41", glz::custom<&T::set_gravity, &T::get_gravity>,
            "42", glz::custom<&T::set_earthMasses, &T::get_earthMasses>,
            "43", glz::custom<&T::set_radius, &T::get_solarRadius>,
            "44", glz::custom<&T::set_surfacePressure, &T::get_surfacePressure>,
            "45", glz::custom<&T::set_volcanismType, &T::get_volcanismType>,
            "46", glz::custom<&T::set_atmosphereType, &T::get_atmosphereType>,
            "47", glz::custom<&T::set_atmosphereComposition, &T::get_atmosphereComposition>,
            "48", glz::custom<&T::set_solidComposition, &T::get_solidComposition>,
            "49", glz::custom<&T::set_materials, &T::get_materials>,
            "50", glz::custom<&T::set_terraformingState, &T::get_terraformingState>,
            "51", glz::custom<&T::set_reserveLevel, &T::get_reserveLevel>
    );
};

template <>
struct glz::meta<db::StarSystemJS>
{
    using T = db::StarSystemJS;

    static constexpr auto value = glz::object(
            "1", &T::id64,
            "2", &T::name,
            "3", &T::coords,
            "4", &T::allegiance,
            "5", &T::government,
            "6", &T::primaryEconomy,
            "7", &T::secondaryEconomy,
            "8", &T::security,
            "9", &T::population,
            "10", &T::bodyCount,
            "11", &T::updated_at,
            "12", &T::controllingFaction,
            "13", &T::factions,
            "14", &T::powerState,
            "15", &T::powerConflictProgress,
            "16", &T::powers,
            "17", &T::controllingPower,
            "18", &T::powerStateControlProgress,
            "19", &T::powerStateReinforcement,
            "20", &T::powerStateUndermining,
            "21", &T::thargoidWar,
            "22", &T::bodies
    );

    template <class T>
    static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&) {
        using V = std::decay_t<T>;
        if constexpr (std::same_as<V, Timestamp>) {
            return key == "updated_at" && value.time_since_epoch().count();
        }
        return false;
    }
};

namespace db {

void test_beve(StarSystemJS& ss_js) {
    glz::error_ctx err;
    std::string buffer;

    err = glz::write_cbor(ss_js, buffer);
    if (err)
        LOG_INFO("Error: {}", glz::format_error(err));

}

}
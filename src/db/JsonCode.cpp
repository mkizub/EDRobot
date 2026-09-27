//
// Created by mkizub on 09.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include <glaze/json.hpp>
#include <glaze/json/flatten_map.hpp>
#include <zlib.h>

const int FMT = glz::JSON;

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

namespace glz
{

template <js::IsEnum E>
struct from<FMT, E>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        skip_ws<Opts>(ctx, it, end);
        if (*it == '\"') {
            std::string str{};
            parse<FMT>::op<Opts>(str, ctx, it, end);
            auto* ptr = E::get_ptr(str);
            if (!ptr)
                ptr = E::Decl::instance.addNewValue(str);
            value = E(ptr);
        }
        else if (*it == 'n' && strncmp(it,"null",4)==0) { // glz::match<"null",Opts>(ctx, it, end)
            value = {};
            it += 4;
        }
        else  {
            auto* ptr = E::get_ptr(it);
            if (!ptr)
                ptr = E::Decl::instance.addNewValue(it);
            value = E(ptr);
        }
    }
};

template <js::IsEnum E>
struct to<FMT, E>
{
    template <auto Opts>
    static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
    {
        if (!value.has_value())
            serialize<FMT>::op<Opts>(nullptr, ctx, b, ix);
        else
            serialize<FMT>::op<Opts>(value.sv(), ctx, b, ix);
    }
};

template <>
struct from<FMT, db::BodyParentJS>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        glz::ordered_small_map<int> m;
        parse<FMT>::op<Opts>(m, ctx, it, end);
        auto bgn = m.begin();
        auto tp = JsNavType::get(bgn->first);
        value = db::BodyParentJS{tp, bgn->second};
    }
};

template <>
struct to<FMT, db::BodyParentJS>
{
    template <auto Opts>
    static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
    {
        glz::ordered_small_map<int> m;
        m.emplace(value.type, value.bodyId);
        serialize<FMT>::op<Opts>(m, ctx, b, ix);
    }
};

template <>
struct from<FMT, js::symbol>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        skip_ws<Opts>(ctx, it, end);
        std::string str{};
        parse<FMT>::op<Opts>(str, ctx, it, end);
        value = js::symbol(str);
    }
};

template <>
struct to<FMT, js::symbol>
{
    template <auto Opts>
    static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
    {
        if (!value.has_value())
            serialize<FMT>::op<Opts>(nullptr, ctx, b, ix);
        else
            serialize<FMT>::op<Opts>(value.sv(), ctx, b, ix);
    }
};

template <>
struct from<FMT, Timestamp>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        skip_ws<Opts>(ctx, it, end);
        std::string str{};
        parse<FMT>::op<Opts>(str, ctx, it, end);
        parseTimestampString(str, value);
    }
};

template <>
struct to<FMT, Timestamp>
{
    template <auto Opts>
    static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
    {
        std::string str = formatTimestampString(value);
        serialize<FMT>::op<Opts>(str, ctx, b, ix);
    }
};

} // namespace glz

struct my_opts : public glz::opts {
    uint32_t format = FMT;
    bool skip_null_members_on_read = true;
};


template <>
struct glz::meta<db::MarketLineJS>
{
    using T = db::MarketLineJS;

    static constexpr auto value  = glz::object(
            &T::commodityId,
            &T::demand,
            &T::supply,
            &T::buyPrice,
            &T::sellPrice,
            "name", glz::skip(),
            "symbol", glz::skip(),
            "category", glz::skip()
    );
};

template <>
struct glz::meta<db::MarketJS>
{
    using T = db::MarketJS;
    static constexpr auto value  = glz::object(
            &T::commodities,
            "updateTime", &T::updated,
            "prohibitedCommodities", glz::skip()
    );
};


template <>
struct glz::meta<db::StationJS>
{
    using T = db::StationJS;

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
        if constexpr (glz::string_t<T>) {
            return value.empty();
        }
        if constexpr (glz::is_specialization_v<std::decay_t<T>, js::Enum>) {
            return value->ptr != nullptr;
        }

        return false;
    }

    static constexpr auto value  = glz::object(
            "id", &T::marketId,
            &T::type,
            &T::name,
            "updateTime", &T::updated_at,
            &T::realName,
            &T::carrierName,
            &T::controllingFaction,
            &T::controllingFactionState,
            &T::distanceToArrival,
            &T::primaryEconomy,
            "secondaryEconomy", glz::skip(),
            "economies", &T::set_economies,
            &T::allegiance,
            &T::government,
            &T::services,
            &T::state,
            &T::latitude,
            &T::longitude,
            &T::landingPads,
            &T::carrierDockingAccess,
            &T::market,
            "shipyard", glz::skip(),
            "outfitting", glz::skip()
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
            "id64", glz::skip(),
            &T::type,
            &T::bodyId,
            &T::name,
            &T::subType,
            &T::parents,
            &T::orbitalPeriod,
            &T::semiMajorAxis,
            &T::orbitalEccentricity,
            &T::orbitalInclination,
            &T::argOfPeriapsis,
            &T::meanAnomaly,
            &T::ascendingNode,
            &T::distanceToArrival,
            &T::surfaceTemperature,
            &T::rotationalPeriod,
            &T::axialTilt,
            "rotationalPeriodTidallyLocked", &T::tidalLock,
            "updateTime", &T::updated_at,
            &T::timestamps,
            &T::stations,
            "belts", glz::skip(),
            "rings", glz::skip(),
            "signals", glz::skip(),

            // star part
            "mainStar",          glz::custom<&T::set_mainStar, &T::get_mainStar>,
            "age",               glz::custom<&T::set_age, &T::get_age>,
            "spectralClass",     glz::skip(), //glz::custom<&T::set_spectralClass, &T::get_spectralClass>,
            "luminosity",        glz::custom<&T::set_luminosity, &T::get_luminosity>,
            "absoluteMagnitude", glz::custom<&T::set_absoluteMagnitude, &T::get_absoluteMagnitude>,
            "solarMasses",       glz::custom<&T::set_solarMasses, &T::get_solarMasses>,
            "solarRadius",       glz::custom<&T::set_solarRadius, &T::get_solarRadius>,

            // plant part
            "isLandable",                    glz::custom<&T::set_isLandable, &T::get_isLandable>,
            "gravity",                       glz::custom<&T::set_gravity, &T::get_gravity>,
            "earthMasses",                   glz::custom<&T::set_earthMasses, &T::get_earthMasses>,
            "radius",                        glz::custom<&T::set_radius, &T::get_radius>,
            "surfacePressure",               glz::custom<&T::set_surfacePressure, &T::get_surfacePressure>,
            "volcanismType",                 glz::custom<&T::set_volcanismType, &T::get_volcanismType>,
            "atmosphereType",                glz::custom<&T::set_atmosphereType, &T::get_atmosphereType>,
            "atmosphereComposition",         glz::custom<&T::set_atmosphereComposition, &T::get_atmosphereComposition>,
            "solidComposition",              glz::custom<&T::set_solidComposition, &T::get_solidComposition>,
            "materials",                     glz::custom<&T::set_materials, &T::get_materials>,
            "terraformingState",             glz::custom<&T::set_terraformingState, &T::get_terraformingState>,
            "reserveLevel",                  glz::custom<&T::set_reserveLevel, &T::get_reserveLevel>
    );
};

template <>
struct glz::meta<db::StarSystemJS>
{
    using T = db::StarSystemJS;

    static constexpr auto modify  = glz::object(
            "id64", &T::address,
            "date", &T::updated_at,
            "rings", glz::skip(),
            "signals", glz::skip()
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

void checkStation(StationJS& st, bool is_planetary) {
    auto& type = st.type;
    if (!type.has_value()) {
        if (is_planetary)
            st.type = toJsNavType(TypeNav::PlanetaryInstallation);
        else
            st.type = toJsNavType(TypeNav::SpaceInstallation);
        //LOG_ERROR("Error: Station '{}' has no type", st.name);
        return;
    }
    if (!st.realName.empty()) {
        if (st.name == "System Colonisation Ship") {
            st.type = toJsNavType(TypeNav::ColonisationShip);
            st.name = "System Colonisation Ship " + st.realName;
            st.realName.clear();
        }
        else if (toTypeNav(st.type) == TypeNav::SpaceConstrDepot) {
            //st.name = st.realName;
            st.name = "Orbital Construction Site: " + st.realName;
            st.realName.clear();
        }
        else if (toTypeNav(st.type) == TypeNav::PlanetaryConstrDepot) {
            //st.name = st.realName;
            st.name = "Planetary Construction Site: " + st.realName;
            st.realName.clear();
        }
        else
            LOG_ERROR("Error: Station '{}' has real name {}", st.name, st.realName);
    }
    if (!st.carrierName.empty()) {
        if (toTypeNav(st.type) == TypeNav::FleetCarrier) {
            if (st.name.size() == 7 && st.name[3] == '-') {
                st.name = st.carrierName + " " + st.name;
                st.carrierName.clear();
            }
            else if (st.name.size() == 4) {
                st.type = toJsNavType(TypeNav::SquadronCarrier);
                st.name = st.carrierName + " | " + st.name;
                st.carrierName.clear();
            }
            else
                LOG_ERROR("Error: Carrier '{}' has carrier name {}", st.name, st.carrierName);
        }
        else
            LOG_ERROR("Error: Station '{}' with type {} has carrier name {}", st.name, std::string_view(st.type), st.carrierName);
    }
    if (toTypeNav(st.type) == TypeNav::FleetCarrier) {
        if (st.name.size() == 4) {
            st.type = toJsNavType(TypeNav::SquadronCarrier);
        } else {
            st.controllingFaction.clear();
        }
        st.economies.clear();
    }
}

void checkBody(BodyJS& body) {
    auto type = body.type;
    if (!type.has_value()) {
        LOG_ERROR("Error: Body '{}' has no type", body.name);
        return;
    }
    if (type.sv() == "Star") {
        if (!body.getStarPart()) {
            //LOG_WARNING("Body '{}' with type {} has no star info", body.name, type.sv());
        }
        if (body.getPlanetPart()) {
            LOG_ERROR("Body '{}' with type {} has planet info", body.name, type.sv());
        }
    }
    else if (type.sv() == "Planet") {
        if (body.getStarPart()) {
            LOG_ERROR("Body '{}' with type {} has star info", body.name, type.sv());
        }
        if (!body.getPlanetPart()) {
            //LOG_WARNING("Body '{}' with type {} has no planet info", body.name, type.sv());
        }
    }
    else {
        if (body.getPlanetPart()) {
            LOG_ERROR("Body '{}' with type {} has planet info", body.name, type.sv());
        }
        if (body.getStarPart()) {
            LOG_ERROR("Body '{}' with type {} has star info", body.name, type.sv());
        }
    }
    for (auto& st : body.stations) {
        checkStation(st, true);
    }
}

void fixupStarSystem(StarSystemJS& ss) {
    for (auto& b : ss.bodies) {
        checkBody(b);
    }
    for (auto& st : ss.stations) {
        checkStation(st, false);
    }
}

#ifdef NDEBUG
#define OPTS .error_on_unknown_keys = false
#else
#define OPTS
#endif

bool parseSpanshSystemDump(StarSystemJS& ss, const std::string& text) {
    std::map<std::string,StarSystemJS> systems;
    glz::context ctx{};
    glz::error_ctx err = glz::read<glz::opts{OPTS}>(systems, text, ctx);

    if (err) {
        LOG_ERROR("Deserialization Error: {}", glz::format_error(err));
        return false;
    }
    if (!systems.contains("system")) {
        LOG_ERROR("Expecting one system in json text");
        return false;
    }
    ss = std::move(systems.at("system"));
    fixupStarSystem(ss);
    return true;
}

bool parseSpanshSystemEntry(StarSystemJS& ss, const std::string& text) {
    glz::context ctx{};
    glz::error_ctx err = glz::read<glz::opts{OPTS}>(ss, text, ctx);
    if (err) {
        LOG_ERROR("Serialization Error: {}", glz::format_error(err));
        return false;
    }
    fixupStarSystem(ss);
    return true;
}

}
//
// Created by mkizub on 09.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include "JsEnum.h"
#include <glaze/json.hpp>
#include <glaze/json/flatten_map.hpp>
#include <zlib.h>

const int FMT = glz::JSON;

namespace glz
{

template <JsEnumDeclType E>
struct from<FMT, db::JsEnum<E>>
{
    template <auto Opts>
    static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
    {
        skip_ws<Opts>(ctx, it, end);
        if (*it == '\"') {
            std::string str{};
            parse<FMT>::op<Opts>(str, ctx, it, end);
            value.ptr = E::instance.get(str);
            if (!value.ptr)
                value.ptr = E::instance.addNewValue(str);
        }
        else if (*it == 'n' && strncmp(it,"null",4)==0) { // glz::match<"null",Opts>(ctx, it, end)
            value.ptr = nullptr;
            it += 4;
        }
        else  {
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
            serialize<FMT>::op<Opts>(nullptr, ctx, b, ix);
        else if constexpr (Opts.js_enum_as_number)
            serialize<FMT>::op<Opts>(value.ptr->id, ctx, b, ix);
        else
            serialize<FMT>::op<Opts>(value.ptr->str, ctx, b, ix);
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

struct enum_as_num_opts : public glz::opts {
    uint32_t format = FMT;
    bool js_enum_as_number = true;
};

struct enum_as_str_opts : public glz::opts {
    uint32_t format = FMT;
    bool js_enum_as_number = false;
};

struct my_opts : public glz::opts {
    uint32_t format = FMT;
    bool skip_null_members_on_read = true;
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
            &T::rotationalPeriodTidallyLocked,
            "updateTime", &T::updated_at,
            &T::timestamps,
            "belts", glz::skip(),
            "rings", glz::skip(),
            "signals", glz::skip(),
            "stations", glz::skip(),

            // star part
            "mainStar",          glz::custom<&T::set_mainStar, &T::get_mainStar>,
            "age",               glz::custom<&T::set_age, &T::set_age>,
            "spectralClass",     glz::custom<&T::set_spectralClass, &T::set_spectralClass>,
            "luminosity",        glz::custom<&T::set_luminosity, &T::set_luminosity>,
            "absoluteMagnitude", glz::custom<&T::set_absoluteMagnitude, &T::set_absoluteMagnitude>,
            "solarMasses",       glz::custom<&T::set_solarMasses, &T::set_solarMasses>,
            "solarRadius",       glz::custom<&T::set_solarRadius, &T::set_solarRadius>,

            // plant part
            "isLandable",                    glz::custom<&T::set_isLandable, &T::set_isLandable>,
            "gravity",                       glz::custom<&T::set_gravity, &T::set_gravity>,
            "earthMasses",                   glz::custom<&T::set_earthMasses, &T::set_earthMasses>,
            "radius",                        glz::custom<&T::set_radius, &T::set_solarRadius>,
            "surfacePressure",               glz::custom<&T::set_surfacePressure, &T::set_surfacePressure>,
            "volcanismType",                 glz::custom<&T::set_volcanismType, &T::set_volcanismType>,
            "atmosphereType",                glz::custom<&T::set_atmosphereType, &T::set_atmosphereType>,
            "atmosphereComposition",         glz::custom<&T::set_atmosphereComposition, &T::set_atmosphereComposition>,
            "solidComposition",              glz::custom<&T::set_solidComposition, &T::set_solidComposition>,
            "materials",                     glz::custom<&T::set_materials, &T::set_materials>,
            "terraformingState",             glz::custom<&T::set_terraformingState, &T::set_terraformingState>,
            "reserveLevel",                  glz::custom<&T::set_reserveLevel, &T::set_reserveLevel>
    );
};

template <>
struct glz::meta<db::StarSystemJS>
{
    using T = db::StarSystemJS;

    static constexpr auto modify  = glz::object(
            "date", &T::updated_at,
            "rings", glz::skip(),
            "signals", glz::skip(),
            "stations", glz::skip()
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

void checkBody(BodyJS& body) {
    auto type = body.type;
    if (!type) {
        LOG_ERROR("Error: Body '{}' has no type", body.name);
        return;
    }
    if (type.ptr->str == "Star") {
        if (!body.getStarPart()) {
            LOG_WARNING("Error: Body '{}' with type {} has no star info", body.name, type.ptr->str);
        }
        if (body.getPlanetPart()) {
            LOG_ERROR("Error: Body '{}' with type {} has planet info", body.name, type.ptr->str);
        }
    }
    else if (type.ptr->str == "Planet") {
        if (body.getStarPart()) {
            LOG_ERROR("Error: Body '{}' with type {} has star info", body.name, type.ptr->str);
        }
        if (!body.getPlanetPart()) {
            LOG_WARNING("Error: Body '{}' with type {} has no planet info", body.name, type.ptr->str);
        }
    }
    else {
        if (body.getPlanetPart()) {
            LOG_ERROR("Error: Body '{}' with type {} has planet info", body.name, type.ptr->str);
        }
        if (body.getStarPart()) {
            LOG_ERROR("Error: Body '{}' with type {} has star info", body.name, type.ptr->str);
        }
    }
//    for (const auto& [key,val] : body.timestamps) {
//        if (!JsTimestamps::instance.get(key)) {
//            LOG_ERROR("Error: Unknown body timestamp key \"{}\"", key);
//            JsTimestamps::instance.addNewValue(key);
//        }
//    }
//    for (auto& p : body.parents) {
//        for (const auto &[key, val]: p) {
//            if (!JsParentBodyType::instance.get(key)) {
//                LOG_ERROR("Error: Unknown parent body type \"{}\"", key);
//                JsParentBodyType::instance.addNewValue(key);
//            }
//        }
//    }
//    for (const auto& [key,val] : body.atmosphereComposition) {
//        if (!JsAtmosphereType::instance.get(key)) {
//            LOG_ERROR("Error: Unknown atmosphere composition type \"{}\"", key);
//            JsAtmosphereType::instance.addNewValue(key);
//        }
//    }
//    for (const auto& [key,val] : body.solidComposition) {
//        if (!JsSolidType::instance.get(key)) {
//            LOG_ERROR("Error: Unknown solid composition type \"{}\"", key);
//            JsSolidType::instance.addNewValue(key);
//        }
//    }
//    for (const auto& [key,val] : body.materials) {
//        if (!JsMaterials::instance.get(key)) {
//            LOG_ERROR("Error: Unknown planet material type \"{}\"", key);
//            JsMaterials::instance.addNewValue(key);
//        }
//    }
}

void checkStarSystem(StarSystemJS& ss) {
//    for (const auto& [key,val] : ss.timestamps) {
//        if (!JsTimestamps::instance.get(key)) {
//            LOG_ERROR("Error: Unknown system timestamp key \"{}\"", key);
//            JsTimestamps::instance.addNewValue(key);
//        }
//    }
    for (auto& b : ss.bodies) {
        checkBody(b);
    }
}

void test_json_system(glz::context& ctx, std::string& strbuf) {
    StarSystemJS ss_js;

    glz::error_ctx err;
    err = glz::read<my_opts{}>(ss_js,strbuf,ctx);
    if (err)
        LOG_ERROR("Error: {}", glz::format_error(err));
    checkStarSystem(ss_js);

    strbuf.clear();
}

void test_json(StarSystemJS& ss_js) {
    glz::error_ctx err;
    std::string strbuf;

    err = glz::read_file_json<glz::opts{.error_on_unknown_keys=false}>(ss_js,"10477373803.json",strbuf);
    if (err)
        LOG_ERROR("Error: {}", glz::format_error(err));
    checkStarSystem(ss_js);

    const char* filePath = "D:\\Work\\ED\\EDMapFilter\\galaxy.json.gz";
    std::uintmax_t fizeSize = std::filesystem::file_size(filePath);
    gzFile file = gzopen(filePath, "rb");
    if (!file) {
        LOG_ERROR("Failed to open gzip file.");
        return;
    }
    strbuf.clear();

    // Buffer to hold chunks of uncompressed data
    char buffer[4096];
    int64_t totalBytesRead = 0;
    int bytesRead = 0;
    int reported_pc = 0;
    glz::context ctx{};

    // Read chunk by chunk until EOF
    bool at_file_start = true;
    bool at_system_start = true;
    while ((bytesRead = gzread(file, buffer, sizeof(buffer))) > 0) {
        totalBytesRead += bytesRead;
        for (int i=0; i < bytesRead; i++) {
            if (at_file_start) {
                assert(buffer[i] == '[');
                at_file_start = false;
                continue;
            }
            if (buffer[i] == '\n') {
                if (strbuf.empty() || strbuf == "[") {
                    strbuf.clear();
                    continue;
                }
                test_json_system(ctx,strbuf);
                strbuf.clear();
                at_system_start = true;
                continue;
            }
            if (at_system_start) {
                if (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n')
                    continue;
                at_system_start = false;
                assert(buffer[i] == ',' || buffer[i] == '{' || buffer[i] == ']');
                if (buffer[i] == ',')
                    continue;
                if (buffer[i] == ']') {
                    LOG_ERROR("Parsed galaxy file completely");
                    test_json_system(ctx,strbuf);
                    strbuf.clear();
                    break;
                }
            }
            strbuf.push_back(buffer[i]);
        }
        int pc = totalBytesRead*100 / fizeSize;
        if (pc != reported_pc) {
            reported_pc = pc;
            LOG_ERROR("Parsed galaxy file {}%", pc);
            spdlog::default_logger_raw()->flush();
        }
    }
    strbuf.clear();

    // Check for errors before closing
    if (bytesRead < 0) {
        int errnum;
        LOG_ERROR("Error reading file: {}", gzerror(file, &errnum));
    }

    gzclose(file);
}
}
//
// Created by mkizub on 09.09.2026.
//

#pragma once

#ifndef EDROBOT_JSENUM_H
#define EDROBOT_JSENUM_H

#include <glaze/forward.hpp>

namespace db {

typedef std::initializer_list<std::pair<unsigned, const char *>> IList;

struct JsEnumDecl;

struct JsEnumVal {
    const unsigned id;
    const std::string str;
    const JsEnumDecl* ES;
};
struct JsEnumDecl {

    static constexpr bool is_js_enum_decl = true;

    std::string name;
    std::vector<std::unique_ptr<JsEnumVal>> allValues;
    std::unordered_map<unsigned, JsEnumVal*> mapById;
    std::unordered_map<std::string, JsEnumVal*> mapByName;

    JsEnumDecl(std::string_view name, IList values);

    JsEnumVal* get(unsigned id) const;
    JsEnumVal* get(std::string_view sv) const;

    JsEnumVal* addNewValue(const std::string& v);
};

#define JS_ENUM_DECL(NAME)                                                      \
    struct Js##NAME : public JsEnumDecl {                                       \
        static Js##NAME instance;                                               \
        Js##NAME(IList values) : JsEnumDecl(#NAME, values) {}                   \
    };

JS_ENUM_DECL(Allegiance)
JS_ENUM_DECL(Government)
JS_ENUM_DECL(Economy)
JS_ENUM_DECL(Security)
JS_ENUM_DECL(FactionState)
JS_ENUM_DECL(Power)
JS_ENUM_DECL(PowerState)
JS_ENUM_DECL(ThargoidState)
JS_ENUM_DECL(ParentBodyType)
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

template <typename ES>
struct JsEnum {
    using E = ES;
    JsEnumVal* ptr;
    JsEnum() : ptr(nullptr) {};
    JsEnum(JsEnumVal* p) : ptr(p) { assert(!p || p->ES == &ES::instance); };
    JsEnum(std::string_view str) {
        if (str.empty()) {
            ptr = nullptr;
            return;
        }
        ptr = ES::instance.get(str);
        if (!ptr)
            ptr = ES::instance.addNewValue(str);
    };
    ~JsEnum() noexcept = default;
    operator std::string_view() const noexcept {
        if (!ptr) return {};
        return {ptr->str.data(), ptr->str.size()};
    }
    std::string_view sv() const noexcept {
        if (!ptr) return {};
        return {ptr->str.data(), ptr->str.size()};
    }
    JsEnum& operator=(JsEnumVal* p) { assert(!p || p->ES == &ES::instance); ptr = p; return *this; }
    operator bool() const noexcept { return ptr; }
    bool has_value() const noexcept { return ptr != nullptr; }
    JsEnum& value() const noexcept { return *this; }
    bool operator==(const JsEnum& other) const {
        if (ptr == other.ptr)
            return true;
        unsigned id1 = ptr ? ptr->id : 0;
        unsigned id2 = other.ptr ? other.ptr->id : 0;
        return id1 == id2;
    }
    bool operator<(const JsEnum& other) const {
        if (ptr == other.ptr)
            return false;
        unsigned id1 = ptr ? ptr->id : 0;
        unsigned id2 = other.ptr ? other.ptr->id : 0;
        return id1 < id2;
    }
};

} // namespace db

template<typename T>
concept JsEnumDeclType = requires {
    T::is_js_enum_decl;
};

template <JsEnumDeclType E>
struct glz::meta<db::JsEnum<E>>
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


#endif //EDROBOT_JSENUM_H

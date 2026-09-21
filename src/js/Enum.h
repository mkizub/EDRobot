//
// Created by mkizub on 09.09.2026.
//

#pragma once

#ifndef EDROBOT_ENUM_H
#define EDROBOT_ENUM_H

#include <unordered_map>

#include "../ed_spdlog.h"
#include "symbol.h"

namespace js {

struct EnumID {
    uint8_t id {};
};

struct EnumDeclBase;

template<typename E>
class EnumDecl;

//template <class Type>
//concept IsEnumDecl = std::is_base_of<EnumDecl, Type>::value;

template<typename T>
concept IsEnumDecl = requires(T)
{
    std::is_base_of<EnumDecl<T>, EnumDeclBase>::value;
};

//template<typename T>
//concept IsEnum = requires(T)
//{
//    std::is_base_of<EnumDeclBase, T::ED>::value;
//};

template<typename T>
concept IsEnum = std::is_base_of<EnumID, T>::value;


using IList = std::initializer_list<std::pair<uint8_t, const char *>> ;

struct EnumVal {
    const uint8_t id;
    const bool predefined;
    const js::symbol sym;
    const class EnumDeclBase* ED;
};

struct EnumDeclBase {
    const std::string name;
    const uint8_t maxPredefinedId;
protected:
    std::vector<std::unique_ptr<EnumVal>> allValues;
    std::unordered_map<unsigned, EnumVal*> mapById;
    std::unordered_map<std::string_view, EnumVal*> mapByName;

    EnumDeclBase(std::string_view name, IList values);
public:
    EnumVal* get_ptr(unsigned id) const;
    EnumVal* get_ptr(std::string_view sv) const;
    EnumVal* addNewValue(const std::string& v);
};

template<typename V>
struct EnumDecl : public EnumDeclBase {
    using E = V;
    static EnumDecl instance;

    EnumDecl(std::string_view name, IList values) : EnumDeclBase(name, values) {}

    E get(unsigned id) const;
    E get(std::string_view sv) const;
};

template <IsEnumDecl EDecl>
struct Enum : public EnumID {
    using E = EDecl::E;
    using ED = EDecl;

    static E get(unsigned id) {
        return EDecl::instance.get(id);
    }
    static E get(std::string_view sv) {
        return EDecl::instance.get(sv);
    }

    Enum() = default;
    Enum(nullptr_t) {};
    Enum(EnumVal* p) {
        assert(!p || p->ED == &EDecl::instance);
        id = p ? p->id : 0;
    };
    Enum(std::string_view str) {
        if (str.empty()) {
            id = 0;
            return;
        }
        auto* ptr = EDecl::get_ptr(str);
        if (!ptr)
            ptr = EDecl::instance.addNewValue(str);
        id = ptr->id;
    };
    ~Enum() noexcept = default;
    operator std::string_view() const noexcept {
        if (!id) return {};
        return EDecl::instance.get_ptr(id)->sym.sv();
    }
    [[nodiscard]] std::string_view sv() const noexcept {
        if (!id) return {};
        return EDecl::instance.get_ptr(id)->sym.sv();
    }
    Enum& operator=(EnumVal* p) {
        assert(!p || p->ED == &EDecl::instance);
        id = p ? p->id : 0;
        return *this;
    }
    constexpr uint8_t& emplace() noexcept { id = 0; return id; }
    explicit operator bool() const noexcept { return id; }
    [[nodiscard]] constexpr bool has_value() const noexcept { return id; }
    [[nodiscard]] constexpr bool is_predefined() const noexcept { return id <= EDecl::instance.maxPredefinedId; }
    const uint8_t& value() const noexcept { return id; }
    void reset() noexcept { id = 0; }
    constexpr bool operator==(const Enum& other) const {
        return (id == other.id);
    }
    constexpr bool operator<(const Enum& other) const {
        return (id < other.id);
    }
};


template <typename E>
E EnumDecl<E>::get(unsigned id) const {
    if (auto it = mapById.find(id); it != mapById.end())
        return E(it->second);
    throw std::bad_variant_access();
}

template <typename E>
E EnumDecl<E>::get(std::string_view sv) const {
    if (auto it = mapByName.find(std::string(sv)); it != mapByName.end())
        return E(it->second);
    return {};
}


} // namespace js

#endif //EDROBOT_ENUM_H

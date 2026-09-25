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

template<typename T>
concept IsEnum = std::is_base_of<EnumID, T>::value;
//template<typename Self>
//concept IsEnum = requires { std::is_same_v<Self,Enum<Self>::E>; };

using IList = std::initializer_list<std::pair<uint8_t, const char *>> ;

struct EnumVal {
    const uint8_t id;
    const bool predefined;
    const js::symbol sym;
    const js::vector<js::symbol> aliases;
    const class EnumDeclBase* ED;
};

struct EnumDeclBase {
    const std::string name;
    const uint8_t maxPredefinedId;
protected:
    EnumVal allValues[256];
    std::unordered_map<unsigned, EnumVal*> mapById;
    std::unordered_map<std::string_view, EnumVal*> mapByName;
    const char* allValueNames[256]; // for debugger

    EnumDeclBase(std::string_view name, IList values);
public:
    EnumVal* get_ptr(unsigned id) const {
        if (auto it = mapById.find(id); it != mapById.end())
            return it->second;
        return nullptr;
    }
    EnumVal* get_ptr(std::string_view sv) const {
        if (auto it = mapByName.find(sv); it != mapByName.end())
            return it->second;
        return nullptr;
    }
    EnumVal* addNewValue(const std::string& v);
};

template <typename Self>
    //requires std::is_same_v<Self,Enum<Self>::E>
struct Enum : public EnumID {
    using E = Self;
    struct Decl : public EnumDeclBase {
        static Decl instance;
    private:
        Decl(std::string_view name, IList values) : EnumDeclBase(name, values) {}
    };

    static E get(unsigned id) {
        return E(Decl::instance.get_ptr(id));
    }
    static E get(std::string_view sv) {
        return E(Decl::instance.get_ptr(sv));
    }
    static EnumVal* get_ptr(unsigned id) {
        return Decl::instance.get_ptr(id);
    }
    static EnumVal* get_ptr(std::string_view sv) {
        return Decl::instance.get_ptr(sv);
    }

    Enum() = default;
    Enum(nullptr_t) {};
    Enum(EnumVal* p) {
        assert(!p || p->ED == &Decl::instance);
        id = p ? p->id : 0;
    };
//    Enum(std::string_view str) {
//        if (str.empty()) {
//            id = 0;
//            return;
//        }
//        auto* ptr = Decl::get_ptr(str);
//        if (!ptr)
//            ptr = Decl::instance.addNewValue(str);
//        id = ptr->id;
//    };
    ~Enum() noexcept = default;
    operator std::string_view() const noexcept {
        if (!id) return {};
        return Decl::instance.get_ptr(id)->sym.sv();
    }
    [[nodiscard]] std::string_view sv() const noexcept {
        if (!id) return {};
        return Decl::instance.get_ptr(id)->sym.sv();
    }
    Enum& operator=(EnumVal* p) {
        assert(!p || p->ED == &Decl::instance);
        id = p ? p->id : 0;
        return *this;
    }
    constexpr uint8_t& emplace() noexcept { id = 0; return id; }
    explicit operator bool() const noexcept { return id; }
    [[nodiscard]] constexpr bool has_value() const noexcept { return id; }
    [[nodiscard]] constexpr bool is_predefined() const noexcept { return id <= Decl::instance.maxPredefinedId; }
    const uint8_t& value() const noexcept { return id; }
    void reset() noexcept { id = 0; }
    constexpr bool operator==(const Enum& other) const {
        return (id == other.id);
    }
    constexpr bool operator<(const Enum& other) const {
        return (id < other.id);
    }
};


} // namespace js

#endif //EDROBOT_ENUM_H

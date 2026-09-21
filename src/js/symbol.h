//
// Created by mkizub on 20.09.2026.
//

#pragma once

#ifndef EDROBOT_JS_SYMBOL_H
#define EDROBOT_JS_SYMBOL_H

#include <string>
#include <string_view>
#include <unordered_set>
#include <ostream>

namespace js {

struct string_hash {
    using is_transparent = void; // Enables heterogeneous lookup

    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
};

class symbol {
private:
    static std::unordered_set<std::string, string_hash, std::equal_to<void>>& getSymbolSet();

    const std::string* ptr {};

public:
    static void DefineSymbols(std::initializer_list<std::string_view> init) {
        getSymbolSet().insert(init.begin(), init.end());
    }
    static const std::string* FindString(std::string_view sv) {
        auto& symbolSet = getSymbolSet();
        if (const auto& it = symbolSet.find(sv); it != symbolSet.end())
            return &(*it);
        return nullptr;
    }


    symbol() = default;
    symbol(const std::string& str) {
        ptr = &(*getSymbolSet().emplace(str).first);
    }
    symbol(const std::string_view& sv) : symbol(std::string(sv)) {
    }

    constexpr ~symbol() noexcept = default;

    constexpr symbol& operator=(const symbol& str) noexcept = default;

    symbol& operator=(const std::string_view& sv) noexcept {
        ptr = &(*getSymbolSet().emplace(std::string(sv)).first);
        return *this;
    }
    symbol& operator=(std::nullptr_t) noexcept {
        ptr = nullptr;
        return *this;
    }

    const char& operator[](uint32_t pos) const {
        return ptr->operator[](pos);
    }

    constexpr bool empty() const {
        return ptr ? ptr->empty() : true;
    }
    constexpr const uint32_t size() const {
        return ptr ? ptr->size() : 0;
    }
    constexpr const uint32_t length() const {
        return ptr ? ptr->size() : 0;
    }
    constexpr const char* data() const {
        return ptr ? ptr->data() : nullptr;
    }
    constexpr const char* c_str() const {
        return ptr ? ptr->c_str() : nullptr;
    }

    constexpr operator std::string_view() const noexcept {
        if (!ptr)
            return {};
        return *ptr;
    }
    constexpr std::string_view sv() const noexcept {
        if (!ptr)
            return {};
        return *ptr;
    }
    constexpr const std::string& str() const noexcept {
        if (!ptr)
            return {};
        return *ptr;
    }

    constexpr void clear() noexcept {
        ptr = nullptr;
    }

    constexpr bool has_value() noexcept {
        return ptr != nullptr;
    }

    constexpr void swap(symbol& other) noexcept {
        std::swap(ptr, other.ptr);
    }

    constexpr int compare(const symbol& other) const {
        if (ptr == other.ptr)
            return 0;
        return sv().compare(other.sv());
    }

    friend constexpr bool operator==(const symbol& lhs, const symbol& rhs);
};

constexpr bool operator==(const symbol& lhs, const symbol& rhs) {
    return (lhs.ptr == rhs.ptr);
}
constexpr bool operator==(const symbol& lhs, std::string_view& rhs) {
    return lhs.sv()== rhs;
}
constexpr bool operator==(const std::string_view& lhs, symbol& rhs) {
    return lhs == rhs.sv();
}
constexpr std::strong_ordering operator<=>(const symbol& lhs, const symbol& rhs) {
    return std::operator<=>(lhs.sv(), rhs.sv());
}

} // namespace js


template <>
constexpr void std::swap(js::symbol& lhs, js::symbol& rhs) noexcept {
    lhs.swap(rhs);
}

std::ostream& operator<<(std::ostream& os, const js::symbol& str) {
    if (!str.empty())
        os << str.sv();
    return os;
}

#endif // EDROBOT_JS_SYMBOL_H

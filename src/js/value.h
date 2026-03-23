//
// Created by mkizub on 28.02.2026.
//

#pragma once

#ifndef EDROBOT_VALUE_H
#define EDROBOT_VALUE_H

#include <string>
#include <vector>
#include <variant>
#include <initializer_list>

#include "internal/impl.h"

namespace js {

template<unsigned N, bool M>
class ref;

template <bool IsConst>
class object_iterator;

class ordered_range;

/**
 * @brief A class to hold JSON value
 */
class value
{
private:
    enum type_enum {
        TYPE_NULL,
        TYPE_BOOLEAN,
        TYPE_INTEGER,
        TYPE_FLOATING,
        TYPE_STRING,
        TYPE_ARRAY,
        TYPE_OBJECT,
    }; // type;
#pragma pack(push, 1)
    struct obj_key {
        unsigned length :  8;
        unsigned index  : 24;
        union {
            char buff[4+8+8];
            struct {
                uint32_t length;
                const char *ptr;
                // maybe add allocator
            } large;
        };
        obj_key(int idx, std::string_view sv) {
#ifndef NDEBUG
            std::memset(buff, 0, sizeof(buff));
#endif
            index = (unsigned char)std::clamp(idx, 0, 255);
            if (sv.empty()) {
                length = 0;
                buff[0] = 0;
            }
            else if (sv.size() < sizeof(buff)) {
                length = sv.size();
                strncpy_s(buff, sizeof(buff), sv.data(), sv.size());
            }
            else {
                length = 255;
                large.length = sv.size();
                auto* tmp = (char*)malloc(sv.size()+1);
                strncpy_s(tmp, sv.size()+1, sv.data(), sv.size()+1);
                large.ptr = tmp;
            }
        }
        obj_key(const obj_key& other) {
            index = other.index;
            if (other.length == 0) {
                length = 0;
                buff[0] = 0;
            }
            else if (other.length < sizeof(buff)) {
                length = other.length;
                strncpy_s(buff, sizeof(buff), other.buff, other.length);
            }
            else {
                length = 255;
                large.length = other.large.length;
                auto* tmp = (char*)malloc(large.length+1);
                strncpy_s(tmp, large.length+1, other.large.ptr, large.length+1);
                large.ptr = tmp;
            }
        }
        obj_key(obj_key&& other) {
            index = other.index;
            if (other.length == 0) {
                length = 0;
                buff[0] = 0;
            }
            else if (other.length < sizeof(buff)) {
                length = other.length;
                strncpy_s(buff, sizeof(buff), other.buff, other.length);
            }
            else {
                length = 255;
                large.length = other.large.length;
                large.ptr = other.large.ptr;
                other.length = 0;
            }
        }

        ~obj_key() {
            if (length >= sizeof(buff))
                free((void *) large.ptr);
        }
        operator const char*() const {
            if (length >= sizeof(buff))
                return large.ptr;
            return buff;
        }
        operator std::string_view() const {
            if (length >= sizeof(buff))
                return {large.ptr, large.length};
            return {buff, length};
        }
        bool operator==(const obj_key& other) const {
            return this->operator std::string_view() == other.operator std::string_view();
        }
        bool operator<(const obj_key& other) const {
            const char* p1 = this->operator const char *();
            const char* p2 = other.operator const char *();
            return strcmp(p1, p2) < 0;
        }
    };
#pragma pack(pop)
    struct obj_val {
        using map_type = std::map<obj_key, value>;

        map_type map;
        unsigned short force_flags {};
        unsigned short key_count {};
        [[nodiscard]] bool empty() const {
            return map.empty();
        }
        [[nodiscard]] bool contains(std::string_view sv) const {
            obj_key key(0, sv);
            return map.contains(key);
        }
        bool operator==(const obj_val& other) const {
            return this->map == other.map;
        }
    };
    struct arr_val {
        using arr_type = std::vector<value>;

        arr_type arr;
        unsigned short force_flags {};
        unsigned short key_count {};
        [[nodiscard]] bool empty() const {
            return arr.empty();
        }
        bool operator==(const arr_val& other) const {
            return this->arr == other.arr;
        }
    };

    template<unsigned N, bool M> friend class ref;
    template <bool C> friend class object_iterator;
    friend class ordered_range;

public:
    using null_type = std::nullptr_t;
    using boolean_type = bool;
    using integer_type = int64_t;
    using unsigned_type = uint64_t;
    using floating_type = double;
    using string_type = std::string;
    using string_p_type = const char*;
    using array_type = arr_val;
    using object_type = obj_val;
    using pair_type = std::pair<std::string_view,value>;
    using json_type = std::string;

    /*================================================================================
     * Construction
     */
public:
    /**
     * @brief JSON value default constructor for "null" type.
     */
    constexpr value() noexcept : content(nullptr) {}

    /**
     * @brief JSON value constructor for "null" type.
     * @param val A dummy argument for nullptr
     */
    constexpr value(null_type val) noexcept : content(nullptr) {}

    /**
     * @brief JSON value constructor for "boolean" type.
     * @param val A boolean value to be set.
     */
    constexpr value(boolean_type val) noexcept : content(val) {}

    /**
     * @brief JSON value constructor for "number" type.
     * @param val A number to be set.
     */
    constexpr value(double val) noexcept : content((floating_type)val) {}
    constexpr value(float val) noexcept : content((floating_type)val) {}

    /**
     * @brief JSON value constructor with integer for "number" type.
     * @param val An integer value to be set.
     */
    constexpr value(int64_t val) noexcept : content((integer_type)val) {}
    constexpr value(int32_t val) noexcept : content((integer_type)val) {}
    constexpr value(int16_t val) noexcept : content((integer_type)val) {}
    constexpr value(int8_t val) noexcept : content((integer_type)val) {}
    constexpr value(uint64_t val) noexcept : content((integer_type)val) {}
    constexpr value(uint32_t val) noexcept : content((integer_type)val) {}
    constexpr value(uint16_t val) noexcept : content((integer_type)val) {}
    constexpr value(uint8_t val) noexcept : content((integer_type)val) {}

    /**
     * @brief JSON value constructor for "string" type.
     * @param val A string value to be set.
     */
    constexpr value(std::string_view val) : content(string_type(val)) {}

    /**
     * @brief JSON value constructor for "string" type.
     * @param val A string value to be set.
     */
    constexpr value(const string_type& val) : content(val) {}

    /**
     * @brief JSON value constructor for "string" type. (const char* version)
     * @param val A string value to be set.
     */
    constexpr value(string_p_type val) : content(string_type(val)) {}

    /**
     * @brief JSON value constructor for "array" type.
     * @param elements An initializer list of elements.
     */
    constexpr explicit value(std::initializer_list<value> elements) : content(array_type()) {
        if (elements.size() > 0) {
            auto &av = std::get<TYPE_ARRAY>(content);
            av.arr.reserve(elements.size());
            for (auto &el: elements) {
                av.arr.push_back(el);
            }
        }
    }

    /**
     * @brief JSON value constructor with key,value pair for "object" type.
     * @param elements An initializer list of key,value pair.
     */
    constexpr explicit value(std::initializer_list<pair_type> elements) : content(object_type()) {
        if (elements.size() > 0) {
            auto &ov = std::get<TYPE_OBJECT>(content);
            for (auto &el: elements) {
                ov.map.emplace(obj_key(ov.key_count++, el.first), el.second);
            }
        }
    }

    /**
     * @brief JSON value copy constructor.
     * @param src A value to be copied from.
     */
    value(const value& src) : content(src.content) {}

    /**
     * @brief JSON value move constructor.
     * @param src A value to be moved from.
     */
    value(value&& src)  noexcept : content(std::move(src.content)) {}

    friend value array(std::initializer_list<value> elements);
    friend value object(std::initializer_list<pair_type> elements);

    /*================================================================================
     * Destruction
     */
public:
    /**
     * @brief JSON value destructor.
     */
    ~value() {
        content = nullptr;
    }

    /*================================================================================
     * Type checks
     */
public:
    /**
     * @brief Check if stored value is null.
     */
    bool is_null() const noexcept { return content.valueless_by_exception() || content.index() == TYPE_NULL; }

    /**
     * @brief Check if type of stored value is boolean.
     */
    bool is_bool() const noexcept { return content.index() == TYPE_BOOLEAN; }

    /**
     * @brief Check if type of stored value is number (includes integer).
     */
    bool is_number() const noexcept { return is_real() || is_int(); }

    /**
     * @brief Check if type of stored value is floating point number.
     */
    bool is_real() const noexcept { return content.index() == TYPE_FLOATING; }

    /**
     * @brief Check if type of stored value is integer.
     */
    bool is_int() const noexcept { return content.index() == TYPE_INTEGER; }

    /**
     * @brief Check if type of stored value is string.
     */
    bool is_string() const noexcept { return content.index() == TYPE_STRING; }

    /**
     * @brief Check if type of stored value is array.
     */
    bool is_array() const noexcept { return content.index() == TYPE_ARRAY; }

    /**
     * @brief Check if type of stored value is object.
     */
    bool is_object() const noexcept { return content.index() == TYPE_OBJECT; }

    /**
     * @brief Check if value is empty (null, empty string, array, object)
     */
    bool empty() const noexcept {
        switch (content.index()) {
        case TYPE_NULL:
            return true;
        case TYPE_STRING:
            return std::get<TYPE_STRING>(content).empty();
        case TYPE_ARRAY:
            return std::get<TYPE_ARRAY>(content).empty();
        case TYPE_OBJECT:
            return std::get<TYPE_OBJECT>(content).empty();
        }
        return false;
    }

    bool has_key(std::string_view key) const noexcept {
        if (content.index() != TYPE_OBJECT)
            return false;
        return std::get<TYPE_OBJECT>(content).contains(key);
    }

    /*================================================================================
     * Type casts
     */
public:
    /**
     * @brief Cast to null
     *
     * @throws std::bad_variant_access if the value is not a null
     */
    [[nodiscard]] null_type as_null() const
    {
        return std::get<TYPE_NULL>(content);
    }

    /**
     * @brief Cast to boolean
     *
     * @throws std::bad_variant_access if the value is not a boolean
     */
    [[nodiscard]] boolean_type as_bool() const
    {
        return std::get<TYPE_BOOLEAN>(content);
    }

    /**
     * @brief Cast to boolean
     *
     * @return default value if not a boolean
     */
    [[nodiscard]] boolean_type as_bool_or(bool default_value=false) const noexcept
    {
        if (is_bool())
            return std::get<TYPE_BOOLEAN>(content);
        return default_value;
    }

    /**
     * @brief Cast to real number
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] auto as_real() const -> floating_type
    {
        if (is_int())
            return static_cast<floating_type>(std::get<TYPE_INTEGER>(content));
        return std::get<TYPE_FLOATING>(content);
    }

    /**
     * @brief Cast to real number
     *
     * @return default value if not a number
     */
    [[nodiscard]] auto as_real_or(floating_type default_value=0.0) const noexcept -> floating_type
    {
        if (is_real())
            return std::get<TYPE_FLOATING>(content);
        if (is_int())
            return static_cast<floating_type>(std::get<TYPE_INTEGER>(content));
        return default_value;
    }

    /**
     * @brief Cast to integer number
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] auto as_int() const -> integer_type
    {
        if (is_real())
            return static_cast<integer_type>(std::get<TYPE_FLOATING>(content));
        return std::get<TYPE_INTEGER>(content);
    }

    /**
     * @brief Cast to integer number
     *
     * @return default value if not a number
     */
    [[nodiscard]] auto as_int_or(integer_type default_value=0) const noexcept -> integer_type
    {
        if (is_int())
            return std::get<TYPE_INTEGER>(content);
        else if (is_real())
            return static_cast<integer_type>(std::get<TYPE_FLOATING>(content));
        return default_value;
    }

    /**
     * @brief Cast to integer number
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] auto as_unsigned() const -> unsigned_type
    {
        if (is_int())
            return std::get<TYPE_INTEGER>(content);
        else if (is_real())
            return static_cast<integer_type>(std::get<TYPE_FLOATING>(content));
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to integer number
     *
     * @return default value if not a number
     */
    [[nodiscard]] auto as_unsigned_or(unsigned_type default_value=0U) const noexcept -> unsigned_type
    {
        if (is_int())
            return std::get<TYPE_INTEGER>(content);
        else if (is_real())
            return static_cast<integer_type>(std::get<TYPE_FLOATING>(content));
        return default_value;
    }

    /**
     * @brief Cast to string
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] const string_type& as_string() const
    {
        return std::get<TYPE_STRING>(content);
    }

    /**
     * @brief Cast to string reference
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] string_type& as_string()
    {
        return std::get<TYPE_STRING>(content);
    }

    /**
     * @brief Cast to string reference
     *
     * @return emoty string if not a string
     */
    [[nodiscard]] const string_type& as_string_or() const noexcept
    {
        if (is_string())
            return std::get<TYPE_STRING>(content);
        static string_type dummy;
        return dummy;
    }

    /**
     * @brief Cast to string reference
     *
     * @return default value if not a string
     */
    [[nodiscard]] string_type as_string_or(std::string_view default_value) const noexcept
    {
        if (is_string())
            return std::get<TYPE_STRING>(content);
        return string_type(default_value);
    }

    /**
     * @brief Cast to array
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] const std::vector<value>& as_array() const
    {
        return std::get<TYPE_ARRAY>(content).arr;
    }

    /**
     * @brief Cast to array reference
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] std::vector<value>& as_array()
    {
        return std::get<TYPE_ARRAY>(content).arr;
    }

    /**
     * @brief Cast to array, return [] if not array
     */
    [[nodiscard]] const std::vector<value>& as_array_or() const noexcept
    {
        if (is_array())
            return std::get<TYPE_ARRAY>(content).arr;
        static std::vector<value> dummy;
        return dummy;
    }

    /**
     * @brief Cast to object
     *
     * @throws std::bad_variant_access if the value is not a object
     */
    [[nodiscard]] const std::map<obj_key, value>& as_object() const
    {
        return std::get<TYPE_OBJECT>(content).map;
    }

    /**
     * @brief Cast to object reference
     *
     * @throws std::bad_variant_access if the value is not a object
     */
    [[nodiscard]] std::map<obj_key, value>& as_object()
    {
        return std::get<TYPE_OBJECT>(content).map;
    }

    /**
     * @brief Cast to object, return {} if not object
     */
    [[nodiscard]] const std::map<obj_key, value>& as_object_or() const noexcept
    {
        if (is_object())
            return std::get<TYPE_OBJECT>(content).map;
        static std::map<obj_key, value> dummy;
        return dummy;
    }

    /**
     * @brief Iterate over object key-value pair
     */
    [[nodiscard]] inline object_iterator<true> key_value() const;

    /**
     * @brief Iterate over object key-value pair in insertion order
     */
    [[nodiscard]] inline ordered_range key_value_ordered() const;

    /*================================================================================
     * Truthy/falsy test
     */
    explicit operator bool() const
    {
        switch (content.index()) {
        default:
        case TYPE_NULL:
            return false;
        case TYPE_BOOLEAN:
            return std::get<TYPE_BOOLEAN>(content);
        case TYPE_INTEGER:
            return std::get<TYPE_INTEGER>(content) != 0;
        case TYPE_FLOATING:
            if (auto f = std::get<TYPE_FLOATING>(content); f != 0 && !std::isnan(f))
                return true;
            return false;
        case TYPE_STRING:
            return !std::get<TYPE_STRING>(content).empty();
        case TYPE_ARRAY:
        case TYPE_OBJECT:
            return true;
        }
    }

    /*================================================================================
     * Array indexer
     */
    [[nodiscard]] const value& at(const int index, const value& default_value) const
    {
        if (is_array()) {
            if (auto& arr=std::get<TYPE_ARRAY>(content); index >= 0 && index < (int)arr.arr.size()) {
                return arr.arr[index];
            }
        }
        return default_value;
    }

    [[nodiscard]] const value& at(const int index) const
    {
        static const value null;
        return at(index, null);
    }

    [[nodiscard]] const value& operator[](const int index) const
    {
        return at(index);
    }

    /*================================================================================
     * Object indexer
     */
    [[nodiscard]] const value& at(std::string_view sv, const value& default_value) const
    {
        if (is_object()) {
            auto& ov = std::get<TYPE_OBJECT>(content);
            obj_key key(0, sv);
            auto iter = ov.map.find(key);
            if (iter != ov.map.end())
                return iter->second;
        }
        return default_value;
    }

    [[nodiscard]] const value& at(const string_type& str) const
    {
        static const value null;
        return at(str, null);
    }

    const value& at(const string_p_type str, const value& default_value) const
    {
        return at(std::string_view(str), default_value);
    }

    const value& at(const string_p_type key) const
    {
        static const value null;
        return at(std::string_view(key), null);
    }

    inline ref<1,true> as_ref(std::string_view key);
    inline ref<1,false> as_cref(std::string_view key) const;
    inline ref<1,true> operator[](std::string_view key);
    inline ref<1,true> operator[](string_p_type key);
    inline ref<1,true> operator[](const string_type& key);
    inline ref<1,false> operator[](std::string_view key) const;
    inline ref<1,false> operator[](string_p_type key) const;
    inline ref<1,false> operator[](const string_type& key) const;

    void erase(std::string_view sv)
    {
        if (is_object()) {
            auto& ov = std::get<TYPE_OBJECT>(content);
            obj_key key(0, sv);
            auto iter = ov.map.find(key);
            if (iter != ov.map.end())
                ov.map.erase(iter);
        }
    }

    /*================================================================================
     * Assignment (Copying)
     */
public:
    /**
     * @brief Copy from another JSON value object.
     * @param src A value object.
     */
    value& operator=(const value& src) { content = src.content; return *this; }

    /**
     * @brief Assign null value.
     * @param null A dummy value.
     */
    value& operator=(null_type null) { content = null; return *this; }

    /**
     * @brief Assign boolean value.
     * @param boolean A boolean value to be set.
     */
    value& operator=(boolean_type boolean) { content = boolean; return *this; }

    /**
     * @brief Assign number value.
     * @param number A number to be set.
     */
    value& operator=(double number) { content = floating_type(number); return *this; }
    value& operator=(float number) { content = floating_type(number); return *this; }

    /**
     * @brief Assign number value by integer type.
     * @param integer A integer number to be set.
     */
    value& operator=(int64_t integer) { content = integer_type(integer); return *this; }
    value& operator=(int32_t integer) { content = integer_type(integer); return *this; }
    value& operator=(int16_t integer) { content = integer_type(integer); return *this; }
    value& operator=(int8_t integer) { content = integer_type(integer); return *this; }
    value& operator=(uint64_t integer) { content = integer_type(integer); return *this; }
    value& operator=(uint32_t integer) { content = integer_type(integer); return *this; }
    value& operator=(uint16_t integer) { content = integer_type(integer); return *this; }
    value& operator=(uint8_t integer) { content = integer_type(integer); return *this; }

    /**
     * @brief Assign string value.
     * @param string A string to be set.
     */
    value& operator=(const string_type& string) { content = string; return *this; }

    /**
     * @brief Assign string value from const char*
     * @param string A string to be set.
     */
    value& operator=(string_p_type string) { content = string_type(string); return *this; }

    /**
     * @brief Assign string value from string_view
     * @param string A string to be set.
     */
    value& operator=(std::string_view string) { content = string_type(string); return *this; }

    /**
     * @brief Assign array value by deep copy.
     * @param elements An array to be set.
     */
    value& operator=(std::initializer_list<value> elements) {
        auto& av = content.emplace<array_type>();
        if (elements.size() > 0) {
            av.arr.reserve(elements.size());
            for (auto &el: elements)
                av.arr.push_back(el);
        }
        return *this;
    }

    /**
     * @brief Assign object value by deep copy.
     * @param object An object to be set.
     */
    value& operator=(std::initializer_list<value::pair_type> elements) {
        auto& ov = content.emplace<object_type>();
        if (elements.size() > 0) {
            for (auto &el: elements) {
                ov.map.emplace(obj_key(ov.key_count++, el.first), el.second);
            }
        }
        return *this;
    }

    value& set(std::string_view sv, value& value) {
        auto& ov = std::get<TYPE_OBJECT>(content);
        obj_key key(ov.key_count, sv);
        auto it = ov.map.find(key);
        if (it == ov.map.end()) {
            auto res = ov.map.emplace(key, value);
            ov.key_count += 1;
            return res.first->second;
        } else {
            it->second = value;
            return it->second;
        }
    }

    force get_flags() const {
        if (is_object()) {
            auto &ov = std::get<TYPE_OBJECT>(content);
            return (force)ov.force_flags;
        }
        else if (is_array()) {
            auto &av = std::get<TYPE_ARRAY>(content);
            return (force)av.force_flags;
        }
        return (force)0;
    }
    value& add_flags(force flags) {
        if (is_object()) {
            auto &ov = std::get<TYPE_OBJECT>(content);
            ov.force_flags |= unsigned(flags);
        }
        else if (is_array()) {
            auto &av = std::get<TYPE_ARRAY>(content);
            av.force_flags |= unsigned(flags);
        }
        return *this;
    }
    value& clear_flags(force flags) {
        if (is_object()) {
            auto &ov = std::get<TYPE_OBJECT>(content);
            ov.force_flags &= ~unsigned(flags);
        }
        else if (is_array()) {
            auto &av = std::get<TYPE_ARRAY>(content);
            av.force_flags &= ~unsigned(flags);
        }
        return *this;
    }

    /**
     * @brief Compare two values
     * @param other A value to compare with
     */
    bool operator==(const value& other) const
    {
        auto idx = content.index();
        if (idx != other.content.index())
            return false;
        switch (idx) {
        case TYPE_NULL:
            return true;
        case TYPE_BOOLEAN:
            return std::get<TYPE_BOOLEAN>(content) == std::get<TYPE_BOOLEAN>(other.content);
        case TYPE_FLOATING:
            return std::get<TYPE_FLOATING>(content) == std::get<TYPE_FLOATING>(other.content);
        case TYPE_INTEGER:
            return std::get<TYPE_INTEGER>(content) == std::get<TYPE_INTEGER>(other.content);
        case TYPE_STRING:
            return std::get<TYPE_STRING>(content) == std::get<TYPE_STRING>(other.content);
        case TYPE_ARRAY:
            return std::get<TYPE_ARRAY>(content) == std::get<TYPE_ARRAY>(other.content);
        case TYPE_OBJECT:
            return std::get<TYPE_OBJECT>(content) == std::get<TYPE_OBJECT>(other.content);
        default:
            return false;
        }
    }

    /**
     * @brief Compare two values
     * @param other A value to compare with
     */
    bool operator!=(const value& other) const {
        return !operator==(other);
    }

    /*================================================================================
     * Parse
     */
private:
    template <impl::flags_type F>
    friend class impl::parser;

    friend impl::parser<0> operator>>(std::istream& istream, value& v);

    /*================================================================================
     * Stringify
     */
    friend class impl::stringifier;

    friend impl::stringifier operator<<(std::ostream& ostream, const value& v);

public:
    template <class... T>
    json_type stringify(T... args) const;

    template <class... T>
    json_type stringify5(T... args) const;

    /*================================================================================
     * Internal data structure
     */
private:
    // nust match type_enum
    std::variant<null_type,boolean_type,integer_type,floating_type,string_type,array_type,object_type> content;
};

/**
 * @brief Make JSON array
 *
 * @param elements An initializer list of elements
 * @return JSON value object
 */
inline value array(std::initializer_list<value> elements)
{
    value v;
    v = elements;
    return v;
}

/**
 * @brief Make JSON object
 *
 * @param elements An initializer list of key:value pairs
 * @return JSON value object
 */
inline value object(std::initializer_list<value::pair_type> elements)
{
    value v;
    v = elements;
    return v;
}

template <unsigned N, bool M>
class ref
{
public:
    using V = std::conditional_t<M, value, const value>;

    V& vref;
    std::string_view keys[N];

private:
    friend class value;
    friend class ref;
    friend class ref<N-1,M>;

    constexpr ref(V& v, std::string_view* keys_ptr)
            : vref(v)
    {
        for (int i=0; i < N; i++)
            keys[i] = keys_ptr[i];
    }

    constexpr ref(V& v, std::string_view keys_ptr[N-1], std::string_view ext_key)
            : vref(v)
    {
        for (int i=0; i < N-1; i++)
            keys[i] = keys_ptr[i];
        keys[N-1] = ext_key;
    }

    ref() = delete;
    ref(const ref&) = delete;
    ref(ref&&) = delete;
    ref& operator=(const ref&) = delete;
    ref& operator=(ref&) = delete;
    ref& operator=(ref&&) = delete;

public:
    ~ref() = default;

    [[nodiscard]] bool exists() const {
        V* v = try_deref();
        return v != nullptr;
    }
    [[nodiscard]] bool empty() const {
        V* v = try_deref();
        return !v || v->empty();
    }
    [[nodiscard]] bool is_null() const {
        V* v = try_deref();
        return v && v->is_null();
    }
    [[nodiscard]] bool is_bool() const {
        V* v = try_deref();
        return v && v->is_bool();
    }
    [[nodiscard]] bool is_number() const {
        V* v = try_deref();
        return v && v->is_number();
    }
    [[nodiscard]] bool is_real() const {
        V* v = try_deref();
        return v && v->is_real();
    }
    [[nodiscard]] bool is_int() const {
        V* v = try_deref();
        return v && v->is_int();
    }
    [[nodiscard]] bool is_string() const {
        V* v = try_deref();
        return v && v->is_string();
    }
    [[nodiscard]] bool is_array() const {
        V* v = try_deref();
        return v && v->is_array();
    }
    [[nodiscard]] bool is_object() const {
        V* v = try_deref();
        return v && v->is_object();
    }
    [[nodiscard]] value::null_type as_null() const {
        V* v = try_deref();
        if (v && !v->is_null()) { throw std::bad_variant_access(); }
        return nullptr;
    }
    [[nodiscard]] value::boolean_type as_bool() const {
        V* v = try_deref();
        if (!v || !v->is_bool()) { throw std::bad_variant_access(); }
        return v->as_bool();
    }
    [[nodiscard]] value::boolean_type as_bool_or(value::boolean_type default_value=false) const noexcept {
        V* v = try_deref();
        if (!v) { return default_value; }
        return v->as_bool_or(default_value);
    }
    [[nodiscard]] auto as_real() const -> value::floating_type {
        V* v = try_deref();
        if (!v || !v->is_number()) { throw std::bad_variant_access(); }
        return v->as_real();
    }
    [[nodiscard]] auto as_real_or(value::floating_type default_value=0.0) const noexcept -> value::floating_type {
        V* v = try_deref();
        if (!v) { return default_value; }
        return v->as_real_or(default_value);
    }
    [[nodiscard]] auto as_int() const -> value::integer_type {
        V* v = try_deref();
        if (!v || !v->is_number()) { throw std::bad_variant_access(); }
        return v->as_int();
    }
    [[nodiscard]] auto as_int_or(value::integer_type default_value=0) const noexcept -> value::integer_type {
        V* v = try_deref();
        if (!v) { return default_value; }
        return v->as_int_or(default_value);
    }
    [[nodiscard]] const value::string_type& as_string() requires (!M) {
        const V* v = try_deref();
        if (!v) { throw std::bad_variant_access(); }
        return v->as_string();
    }
    [[nodiscard]] value::string_type& as_string() requires M {
        V& v = deref();
        if (v.is_null()) { v = ""; }
        if (!v.is_string()) { throw std::bad_variant_access(); }
        return v.as_string();
    }
    [[nodiscard]] value::string_type as_string_or(std::string_view default_value={}) const noexcept {
        V* v = try_deref();
        if (!v) { return value::string_type(default_value); }
        return v->as_string_or(default_value);
    }
    [[nodiscard]] const std::vector<value>& as_array() requires (!M) {
        V* v = try_deref();
        if (!v) { throw std::bad_variant_access(); }
        return v->as_array();
    }
    [[nodiscard]] std::vector<value>& as_array() requires M {
        V& v = deref();
        if (v.is_null()) { v = array({}); }
        if (!v.is_array()) { throw std::bad_variant_access(); }
        return v.as_array();
    }
    [[nodiscard]] const std::vector<value>& as_array_or() const noexcept {
        V* v = try_deref();
        if (!v || !v->is_array()) {
            static std::vector<value> dummy;
            return dummy;
        }
        return v->as_array_or();
    }
    [[nodiscard]] const std::map<value::obj_key, value>& as_object() requires (!M) {
        V* v = try_deref();
        if (!v) { throw std::bad_variant_access(); }
        return v->as_object();
    }
    [[nodiscard]] std::map<value::obj_key, value>& as_object() requires M {
        V& v = deref();
        if (v.is_null()) { v = object({}); }
        if (!v.is_object()) { throw std::bad_variant_access(); }
        return v.as_object();
    }
    [[nodiscard]] const std::map<value::obj_key, value>& as_object_or() const noexcept {
        V* v = try_deref();
        if (!v || !v->is_object()) {
            static std::map<value::obj_key, value> dummy;
            return dummy;
        }
        return v->as_object_or();
    }
    [[nodiscard]] inline object_iterator<true> key_value() const;

    V& operator=(nullptr_t rhs) requires M { return deref() = rhs; }
    V& operator=(bool rhs) requires M { return deref() = rhs; }
    V& operator=(int32_t rhs) requires M { return deref() = rhs; }
    V& operator=(uint32_t rhs) requires M { return deref() = rhs; }
    V& operator=(int64_t rhs) requires M { return deref() = rhs; }
    V& operator=(uint64_t rhs) requires M { return deref() = rhs; }
    V& operator=(int16_t rhs) requires M { return deref() = rhs; }
    V& operator=(uint16_t rhs) requires M { return deref() = rhs; }
    V& operator=(double rhs) requires M { return deref() = rhs; }
    V& operator=(float rhs) requires M { return deref() = rhs; }
    V& operator=(const value::string_type& rhs) requires M { return deref() = rhs; }
    V& operator=(value::string_p_type rhs) requires M { return deref() = rhs; }
    V& operator=(const std::string_view rhs) requires M { return deref() = rhs; }
    V& operator=(const value& rhs) requires M { return deref() = rhs; }

    explicit operator bool() const {
        V* v = try_deref();
        return v && v->operator bool();
    }
    constexpr ref<N+1,true> as_ref(std::string_view key) requires M {
        return {vref, keys, key};
    }
    constexpr ref<N+1,false> as_cref(std::string_view key) {
        return {vref, keys, key};
    }
    constexpr ref<N+1,M> operator[](std::string_view key) {
        if constexpr (M)
            return as_ref(key);
        else
            return as_cref(key);
    }
    constexpr ref<N+1,M> operator[](value::string_p_type key) {
        if constexpr (M)
            return as_ref(key);
        else
            return as_cref(key);
    }
    constexpr ref<N+1,M> operator[](const value::string_type& key) {
        if constexpr (M)
            return as_ref(key);
        else
            return as_cref(key);
    }
    [[nodiscard]] V& operator[](const int index) {
        return operator V&().at(index);
    }
    force get_flags() {
        V* v = try_deref();
        if (!v) return force::none;
        return v->get_flags();
    }
    value& add_flags(force flags) requires M {
        return deref().add_flags(flags);
    }
    value& clear_flags(force flags) requires M {
        return deref().clear_flags(flags);
    }
    [[nodiscard]] constexpr V* try_deref() const {
        V* ptr = &vref;
        for (int idx=0; idx < N; idx++) {
            if (!ptr->is_object())
                return nullptr;
            auto& ov = std::get<value::TYPE_OBJECT>(ptr->content);
            value::obj_key key(0,keys[idx]);
            auto it = ov.map.find(key);
            if (it == ov.map.end())
                return nullptr;
            ptr = &it->second;
        }
        return ptr;
    }
    [[nodiscard]] constexpr const V& deref() requires (!M) {
        V* v = try_deref();
        if (v == nullptr) {
            static V dummy;
            return dummy;
        }
        return *v;
    }
    [[nodiscard]] constexpr V& deref() requires M {
        V* ptr = &vref;
        for (int idx=0; idx < N; idx++) {
            if (ptr->is_null()) {
                *ptr = object({{keys[idx],value()}});
            }
            if (!ptr->is_object())
                throw std::bad_variant_access();
            auto& ov = std::get<value::TYPE_OBJECT>(ptr->content);
            value::obj_key key(ov.key_count,keys[idx]);
            auto it = ov.map.find(key);
            if (it == ov.map.end()) {
                auto res = ov.map.emplace(std::move(key), value());
                ov.key_count++;
                ptr = &res.first->second;
            } else {
                ptr = &it->second;
            }
        }
        return *ptr;
    }
    [[nodiscard]] constexpr /* implicit cast! */ operator const V&() {
        V* v = try_deref();
        if (v == nullptr) {
            static V null;
            return null;
        }
        return *v;
    }
    [[nodiscard]] constexpr /* implicit cast! */ operator V&() requires M {
        return deref();
    }
};


inline ref<1,true> value::as_ref(std::string_view key) {
    return ref<1,true>(*this, &key);
}
inline ref<1,false> value::as_cref(std::string_view key) const {
    return ref<1,false>(*this, &key);
}
inline ref<1,true> value::operator[](std::string_view key) {
    return as_ref(key);
}
inline ref<1,true> value::operator[](value::string_p_type key) {
    return as_ref(key);
}
inline ref<1,true> value::operator[](const value::string_type& key) {
    return as_ref(key);
}
inline ref<1,false> value::operator[](std::string_view key) const {
    return as_cref(key);
}
inline ref<1,false> value::operator[](string_p_type key) const {
    return as_cref(key);
}
inline ref<1,false> value::operator[](const string_type& key) const {
    return as_cref(key);
}



template <bool IsConst>
class object_iterator
{
public:
    using map_type = typename std::conditional<
            IsConst, typename std::add_const<value::obj_val::map_type>::type, typename value::obj_val::map_type>::type;
    using value_type = class value;

    explicit object_iterator(map_type& map)
        : map(map)
    {
        it = map.end();
    }

    std::pair<std::string_view,const value&> operator*() const {
        return {it->first, it->second};
    }

    object_iterator begin() {
        it = map.cbegin();
        return *this;
    }
    std::default_sentinel_t end() {
        return {};
    }

    std::string_view key() const {
        return it->first;
    }

    const value& value() const {
        return it->second;
    }

    object_iterator& operator++() {
        it++;
        return *this;
    }

    //object_iterator operator++(intn) {
    //    auto tmp{ *this };
    //    ++*this;
    //    return tmp;
    //}

    bool operator==(const object_iterator& other) const {
        return it == other.it;
    }
    bool operator==(const std::default_sentinel_t&) const {
        return it == map.end();
    }
private:
    map_type& map;
    map_type::const_iterator it;
};

class ordered_range
{
public:
    using map_type = typename std::add_const<value::obj_val::map_type>::type;
    using vector_type = std::vector<std::pair<const map_type::key_type*,const map_type::mapped_type*>>;
    using value_type = class value;

    explicit ordered_range(map_type& map) {
        array.reserve(map.size());
        for (auto& p : map)
            array.emplace_back(&p.first, &p.second);
        std::sort(array.begin(), array.end(), [](auto& p1, auto& p2)->bool {
            return p1.first->index < p2.first->index;
        });
    }

    struct iterator {
        vector_type::const_iterator it;
        ordered_range& range;

        std::pair<std::string_view,const value&> operator*() const {
            return {key(), value()};
        }

        std::string_view key() const {
            return *it->first;
        }

        const value& value() const {
            return *it->second;
        }

        iterator& operator++() {
            it++;
            return *this;
        }

        bool operator==(const iterator& other) const {
            return it == other.it;
        }
    };

    iterator begin() {
        return {array.cbegin(), *this};
    }
    iterator end() {
        return {array.cend(), *this};
    }


private:
    vector_type array;
    vector_type::const_iterator it;
};

inline object_iterator<true> value::key_value() const {
    return object_iterator<true>(std::get<value::TYPE_OBJECT>(content).map);
}

inline ordered_range value::key_value_ordered() const {
    return ordered_range(std::get<value::TYPE_OBJECT>(content).map);
}

template<unsigned N, bool M>
[[nodiscard]] inline object_iterator<true> ref<N,M>::key_value() const {
    V* v = try_deref();
    if (!v || !v->is_object()) { throw std::bad_variant_access(); }
    return v->key_value();
}

} // namespace js

template <>
struct std::formatter<js::value> : std::formatter<std::string> {
    auto format(const js::value& v, std::format_context& ctx) const {
        return std::formatter<std::string>::format(v.stringify(), ctx);
    }
};

#endif //EDROBOT_VALUE_H

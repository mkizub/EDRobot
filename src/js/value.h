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

#include <tsl/ordered_hash.h>
#include <tsl/ordered_map.h>

#include "internal/impl.h"

namespace js {

template<unsigned N, bool M>
class ref;

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

public:
    using null_type = std::nullptr_t;
    using boolean_type = bool;
    using integer_type = int64_t;
    using unsigned_type = uint64_t;
    using floating_type = double;
    using string_type = std::string;
    using string_p_type = const char*;
    using array_type = std::vector<value>;
    using object_type = tsl::ordered_map<string_type, value, std::hash<string_type>, std::equal_to<>,
    std::allocator<std::pair<string_type, value>>,
    std::vector<std::pair<string_type, value>>,
    std::uint_least32_t>;
    using pair_type = std::pair<string_type, value>;
    // using object_type = std::map<string_type, value>;
    // using pair_type = object_type::value_type;
    using json_type = std::string;

    /*================================================================================
     * Construction
     */
public:
    /**
     * @brief JSON value default constructor for "null" type.
     */
    constexpr value() noexcept : value(nullptr) {}

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
    template<std::floating_point T> requires std::floating_point<T>
    constexpr value(floating_type val) noexcept : content((floating_type)val) {}

    /**
     * @brief JSON value constructor with integer for "number" type.
     * @param val An integer value to be set.
     */
    template<std::integral T> requires std::integral<T>
    constexpr value(T val) noexcept : content((integer_type)val) {}

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
    constexpr explicit value(std::initializer_list<value> elements) : content(array_type(elements)) {}

    /**
     * @brief JSON value constructor with key,value pair for "object" type.
     * @param elements An initializer list of key,value pair.
     */
    constexpr explicit value(std::initializer_list<pair_type> elements) : content(object_type(elements)) {}

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
    ~value() = default;

    /*================================================================================
     * Type checks
     */
public:
    /**
     * @brief Check if stored value is null.
     */
    bool is_null() const noexcept { return content.index() == TYPE_NULL; }

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
    [[nodiscard]] const array_type& as_array() const
    {
        return std::get<TYPE_ARRAY>(content);
    }

    /**
     * @brief Cast to array reference
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] array_type& as_array()
    {
        return std::get<TYPE_ARRAY>(content);
    }

    /**
     * @brief Cast to array, return [] if not array
     */
    [[nodiscard]] const array_type& as_array_or() const noexcept
    {
        if (is_array())
            return std::get<TYPE_ARRAY>(content);
        static array_type dummy;
        return dummy;
    }

    /**
     * @brief Cast to object
     *
     * @throws std::bad_variant_access if the value is not a object
     */
    [[nodiscard]] const object_type& as_object() const
    {
        return std::get<TYPE_OBJECT>(content);
    }

    /**
     * @brief Cast to object reference
     *
     * @throws std::bad_variant_access if the value is not a object
     */
    [[nodiscard]] object_type& as_object()
    {
        return std::get<TYPE_OBJECT>(content);
    }

    /**
     * @brief Cast to object, return {} if not object
     */
    [[nodiscard]] const object_type& as_object_or() const noexcept
    {
        if (is_object())
            return std::get<TYPE_OBJECT>(content);
        static object_type dummy;
        return dummy;
    }

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
            if (auto& arr=std::get<TYPE_ARRAY>(content); index >= 0 && index < (int)arr.size()) {
                return arr[index];
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
    [[nodiscard]] const value& at(const string_type& key, const value& default_value) const
    {
        if (is_object()) {
            auto& map = std::get<TYPE_OBJECT>(content);
            auto iter = map.find(key);
            if (iter != map.end())
                return iter->second;
        }
        return default_value;
    }

    [[nodiscard]] const value& at(const string_type& key) const
    {
        static const value null;
        return at(key, null);
    }

    const value& at(const string_p_type key, const value& default_value) const
    {
        return at(string_type(key), default_value);
    }

    const value& at(const string_p_type key) const
    {
        return at(string_type(key));
    }

    inline ref<1,true> as_ref(std::string_view key);
    inline ref<1,false> as_cref(std::string_view key) const;
    inline ref<1,true> operator[](std::string_view key);
    inline ref<1,true> operator[](string_p_type key);
    inline ref<1,true> operator[](const string_type& key);
    inline ref<1,false> operator[](std::string_view key) const;
    inline ref<1,false> operator[](string_p_type key) const;
    inline ref<1,false> operator[](const string_type& key) const;

    /*================================================================================
     * Assignment (Copying)
     */
public:
    /**
     * @brief Copy from another JSON value object.
     * @param src A value object.
     */
    value& operator=(const value& src)
    {
        content = src.content;
        return *this;
    }

    /**
     * @brief Assign null value.
     * @param null A dummy value.
     */
    value& operator=(null_type null)
    {
        content = null;
        return *this;
    }

    /**
     * @brief Assign boolean value.
     * @param boolean A boolean value to be set.
     */
    value& operator=(boolean_type boolean)
    {
        content = boolean;
        return *this;
    }

    /**
     * @brief Assign number value.
     * @param number A number to be set.
     */
    template <std::floating_point T>
    value& operator=(T number)
    {
        content = number;
        return *this;
    }

    /**
     * @brief Assign number value by integer type.
     * @param integer A integer number to be set.
     */
    template <std::integral T>
    value& operator=(T integer)
    {
        content = integer;
        return *this;
    }

    /**
     * @brief Assign string value.
     * @param string A string to be set.
     */
    value& operator=(const string_type& string)
    {
        content = string;
        return *this;
    }

    /**
     * @brief Assign string value from const char*
     * @param string A string to be set.
     */
    value& operator=(string_p_type string)
    {
        content = string_type(string);
        return *this;
    }

    /**
     * @brief Assign string value from string_view
     * @param string A string to be set.
     */
    value& operator=(std::string_view string)
    {
        content = string_type(string);
        return *this;
    }

    /**
     * @brief Assign array value by deep copy.
     * @param elements An array to be set.
     */
    value& operator=(std::initializer_list<value> elements)
    {
        content.emplace<array_type>(elements);
        return *this;
    }

    /**
     * @brief Assign object value by deep copy.
     * @param object An object to be set.
     */
    value& operator=(std::initializer_list<value::pair_type> elements)
    {
        content.emplace<object_type>(elements);
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
    bool operator!=(const value& other) const
    {
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
    template <impl::flags_type F, impl::indent_type I>
    friend class impl::stringifier;

    friend impl::stringifier<0,0> operator<<(std::ostream& ostream, const value& v);

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
    [[nodiscard]] const value::array_type& as_array() requires (!M) {
        V* v = try_deref();
        if (!v) { throw std::bad_variant_access(); }
        return v->as_array();
    }
    [[nodiscard]] value::array_type& as_array() requires M {
        V& v = deref();
        if (v.is_null()) { v = array({}); }
        if (!v.is_array()) { throw std::bad_variant_access(); }
        return v.as_array();
    }
    [[nodiscard]] const value::array_type& as_array_or() const noexcept {
        V* v = try_deref();
        if (!v || !v->is_array()) {
            static value::array_type dummy;
            return dummy;
        }
        return v->as_array_or();
    }
    [[nodiscard]] const value::object_type& as_object() requires (!M) {
        V* v = try_deref();
        if (!v) { throw std::bad_variant_access(); }
        return v->as_object();
    }
    [[nodiscard]] value::object_type& as_object() requires M {
        V& v = deref();
        if (v.is_null()) { v = object({}); }
        if (!v.is_object()) { throw std::bad_variant_access(); }
        return v.as_object();
    }
    [[nodiscard]] const value::object_type& as_object_or() const noexcept {
        V* v = try_deref();
        if (!v || !v->is_object()) {
            static value::object_type dummy;
            return dummy;
        }
        return v->as_object_or();
    }

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
    [[nodiscard]] constexpr V* try_deref() const {
        V* ptr = &vref;
        for (int idx=0; idx < N; idx++) {
            if (!ptr->is_object())
                return nullptr;
            auto& o = ptr->as_object();
            auto it = o.find(std::string(keys[idx]));
            if (it == o.end())
                return nullptr;
            ptr = &it.value();
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
                *ptr = object({{std::string(keys[idx]),value()}});
            }
            if (!ptr->is_object())
                throw std::bad_variant_access();
            value::object_type &o = ptr->as_object();
            auto res = o.try_emplace(std::string(keys[idx]), value());
            ptr = &res.first.value();
        }
        return *ptr;
    }
    [[nodiscard]] constexpr /* implicit cast! */ operator const V&() {
        V* v = try_deref();
        if (v == nullptr) {
            static V dummy;
            return dummy;
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

} // namespace js

#endif //EDROBOT_VALUE_H

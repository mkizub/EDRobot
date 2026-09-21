//
// Created by mkizub on 28.02.2026.
//

#pragma once

#ifndef EDROBOT_JS_VALUE_H
#define EDROBOT_JS_VALUE_H

#include <string>
#include <vector>
#include <variant>
#include <initializer_list>

#include "internal/impl.h"
#include "internal/key.h"
#include "internal/str.h"
#include "small_map.h"
#include "symbol.h"
#include "vector.h"
#include "Enum.h"

#include <boost/pool/pool_alloc.hpp>

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

    template<unsigned N, bool M> friend class ref;
    template <bool C> friend class object_iterator;
    friend class ordered_range;
    friend class js::impl::str;

    inline constexpr void ctor_erase(TYPE ctor_type) {
        content.integer = 0;
        count = {};
        reserved0 = {};
        reserved1 = {};
        flags = {};
        type = ctor_type;
    }

public:
    using null_type = std::nullptr_t;
    using boolean_type = bool;
    using integer_type = int64_t;
    using unsigned_type = uint64_t;
    using floating_type = double;
    using string_type = std::string_view;
    using array_type = js::vector<value>;
    //using object_type = std::map<impl::key, value, std::less<void>, boost::fast_pool_allocator<std::pair<const impl::key,value>>>;
    using object_type = js::small_map<impl::key, value>;
    using pair_type = std::pair<std::string_view,value>;

    /*================================================================================
     * Construction
     */
public:
    /**
     * @brief JSON value default constructor for "null" type.
     */
    constexpr value() noexcept {
        ctor_erase(TYPE::NIL);
    }

    /**
     * @brief JSON value constructor for "null" type.
     * @param val A dummy argument for nullptr
     */
    constexpr value(null_type val) noexcept {
        ctor_erase(TYPE::NIL);
    }

    /**
     * @brief JSON value constructor for "boolean" type.
     * @param val A boolean value to be set.
     */
    constexpr value(boolean_type val) noexcept {
        ctor_erase(TYPE::BOOL);
        content.boolean = val;
    }

    /**
     * @brief JSON value constructor for "number" type.
     * @param val A number to be set.
     */
    constexpr value(double val) noexcept {
        ctor_erase(TYPE::REAL);
        content.real = val;
    }
    constexpr value(float val) noexcept {
        ctor_erase(TYPE::REAL);
        content.real = static_cast<floating_type>(val);
    }

    /**
     * @brief JSON value constructor with integer for "number" type.
     * @param val An integer value to be set.
     */
    constexpr value(int64_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(int32_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(int16_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(int8_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(uint64_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(uint32_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(uint16_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }
    constexpr value(uint8_t val) noexcept {
        ctor_erase(TYPE::INT);
        content.integer = static_cast<integer_type>(val);
    }

    /**
     * @brief JSON value constructor for "string" type.
     * @param val A string value to be set.
     */
    constexpr value(std::string_view val) noexcept {
        ctor_erase(TYPE::NIL);
        new(&content.str) impl::str(val);
    }

    /**
     * @brief JSON value constructor for "string" type.
     * @param val A string value to be set.
     */
    constexpr value(const std::string& val) : value(std::string_view(val)) {}

    /**
     * @brief JSON value constructor for "string" type. (const char* version)
     * @param val A string value to be set.
     */
    constexpr value(const char* val) : value(std::string_view(val)) {}

    /**
     * @brief JSON value constructor for "array" type.
     * @param elements An initializer list of elements.
     */
    constexpr explicit value(std::initializer_list<value> elements) {
        ctor_erase(TYPE::ARR);
        new(&content.array) array_type();
        if (elements.size() > 0) {
            content.array.reserve(elements.size());
            for (auto &el: elements) {
                content.array.push_back(el);
            }
        }
    }

    /**
     * @brief JSON value constructor with key,value pair for "object" type.
     * @param elements An initializer list of key,value pair.
     */
    constexpr explicit value(std::initializer_list<pair_type> elements) {
        ctor_erase(TYPE::OBJ);
        content.object = new object_type();
        if (elements.size() > 0) {
            for (auto &el: elements) {
                // also increments key count
                content.object->emplace(impl::key(count++, el.first), el.second);
            }
        }
    }

    /**
     * @brief JSON value copy constructor.
     * @param src A value to be copied from.
     */
    constexpr value(const value& src) {
        ctor_erase(TYPE::NIL);
        *this = src;
    }

    /**
     * @brief JSON value move constructor.
     * @param src A value to be moved from.
     */
    constexpr value(value&& src)  noexcept {
        count = src.count;
        reserved0 = {};
        reserved1 = {};
        flags = src.flags;
        type = src.type;

        switch (type) {
        case TYPE::NIL:
        case TYPE::BOOL:
        case TYPE::INT:
            content.integer = src.content.integer;
            break;
        case TYPE::REAL:
            content.real = src.content.real;
            break;
        case TYPE::STR_BUF:
        case TYPE::STR_EXT:
        case TYPE::STR_OWN:
            type = TYPE::NIL;
            content.integer = 0;
            new (&content.str) impl::str(std::move(src.content.str));
            break;
        case TYPE::ARR:
            new (&content.array) array_type(std::move(src.content.array));
            break;
        case TYPE::OBJ:
            content.object = src.content.object;
            break;
        default:
            break;
        }
        src.ctor_erase(TYPE::NIL);
    }

    friend value array(std::initializer_list<value> elements);
    friend value object(std::initializer_list<pair_type> elements);

    /*================================================================================
     * Destruction
     */
public:
    /**
     * @brief JSON value destructor.
     */
    constexpr ~value() {
        release();
    }

    /**
     * @brief Release content
     *
     * @param new_type A new type id
     */
    constexpr void release()
    {
        switch (type) {
        case TYPE::NIL:
        case TYPE::BOOL:
        case TYPE::INT:
        case TYPE::REAL:
            break;
        case TYPE::STR_BUF:
        case TYPE::STR_EXT:
            break;
        case TYPE::STR_OWN:
            if (content.str.ptr) {
                free((void *) content.str.ptr);
                content.str.ptr = nullptr;
            }
            break;
        case TYPE::ARR:
            content.array.~array_type();
            break;
        case TYPE::OBJ:
            std::free(content.object);
            break;
        }
        ctor_erase(TYPE::NIL);
    }

    /*================================================================================
     * Type checks
     */
public:
    /**
     * @brief Check if stored value is null.
     */
    constexpr bool is_null() const noexcept { return type == TYPE::NIL; }

    /**
     * @brief Check if type of stored value is boolean.
     */
    constexpr bool is_bool() const noexcept { return type == TYPE::BOOL; }

    /**
     * @brief Check if type of stored value is number (includes integer).
     */
    constexpr bool is_number() const noexcept { return is_real() || is_int(); }

    /**
     * @brief Check if type of stored value is floating point number.
     */
    constexpr bool is_real() const noexcept { return type == TYPE::REAL; }

    /**
     * @brief Check if type of stored value is integer.
     */
    constexpr bool is_int() const noexcept { return type == TYPE::INT; }

    /**
     * @brief Check if type of stored value is string.
     */
    constexpr bool is_string() const noexcept { return type == TYPE::STR_BUF || type == TYPE::STR_EXT || type == TYPE::STR_OWN; }

    /**
     * @brief Check if type of stored value is array.
     */
    constexpr bool is_array() const noexcept { return type == TYPE::ARR; }

    /**
     * @brief Check if type of stored value is object.
     */
    constexpr bool is_object() const noexcept { return type == TYPE::OBJ; }

    /**
     * @brief Check if value is empty (null, empty string, array, object)
     */
    constexpr bool empty() const noexcept {
        switch (type) {
        case TYPE::NIL:
            return true;
        case TYPE::BOOL:
        case TYPE::INT:
        case TYPE::REAL:
            return false;
        case TYPE::STR_BUF:
            return reinterpret_cast<const char*>(this)[0] == 0;
        case TYPE::STR_EXT:
        case TYPE::STR_OWN:
            return count == 0;
        case TYPE::ARR:
            return content.array.empty();
        case TYPE::OBJ:
            return content.object->empty();
        }
        return false;
    }

    constexpr bool has_key(std::string_view key) const noexcept {
        if (type != TYPE::OBJ)
            return false;
        return content.object->contains(key);
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
    [[nodiscard]] constexpr null_type as_null() const
    {
        if (!is_null()) { throw std::bad_variant_access(); }
        return nullptr;
    }

    /**
     * @brief Cast to boolean
     *
     * @throws std::bad_variant_access if the value is not a boolean
     */
    [[nodiscard]] constexpr boolean_type as_bool() const
    {
        if (!is_bool()) { throw std::bad_variant_access(); }
        return content.boolean;
    }

    /**
     * @brief Cast to boolean
     *
     * @return default value if not a boolean
     */
    [[nodiscard]] constexpr boolean_type as_bool_or(bool default_value=false) const noexcept
    {
        if (is_bool())
            return content.boolean;
        return default_value;
    }

    /**
     * @brief Cast to real number
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] constexpr auto as_real() const -> floating_type
    {
        if (is_real())
            return content.real;
        if (is_int())
            return static_cast<floating_type>(content.integer);
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to real number
     *
     * @return default value if not a number
     */
    [[nodiscard]] constexpr auto as_real_or(floating_type default_value=0.0) const noexcept -> floating_type
    {
        if (is_real())
            return content.real;
        if (is_int())
            return static_cast<floating_type>(content.integer);
        return default_value;
    }

    /**
     * @brief Cast to integer number
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] constexpr auto as_int() const -> integer_type
    {
        if (is_int())
            return content.integer;
        if (is_real())
            return static_cast<integer_type>(content.real);
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to integer number
     *
     * @return default value if not a number
     */
    [[nodiscard]] constexpr auto as_int_or(integer_type default_value=0) const noexcept -> integer_type
    {
        if (is_int())
            return content.integer;
        if (is_real())
            return static_cast<integer_type>(content.real);
        return default_value;
    }

    /**
     * @brief Cast to integer number
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] constexpr auto as_unsigned() const -> unsigned_type
    {
        if (is_int())
            return static_cast<unsigned_type>(content.integer);
        if (is_real())
            return static_cast<unsigned_type>(content.real);
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to integer number
     *
     * @return default value if not a number
     */
    [[nodiscard]] constexpr auto as_unsigned_or(unsigned_type default_value=0U) const noexcept -> unsigned_type
    {
        if (is_int())
            return static_cast<unsigned_type>(content.integer);
        if (is_real())
            return static_cast<unsigned_type>(content.real);
        return default_value;
    }

    /**
     * @brief Cast to string view
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] constexpr const value::string_type as_string() const
    {
        if (is_string())
            return content.str.sv();
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to string view
     *
     * @return default value if not a string
     */
    [[nodiscard]] constexpr const value::string_type as_string_or(std::string_view default_value={}) const noexcept
    {
        if (is_string())
            return content.str.sv();
        return default_value;
    }

    /**
     * @brief Cast to array
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] constexpr const value::array_type& as_array() const
    {
        if (is_array())
            return content.array;
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to array reference
     *
     * @throws std::bad_variant_access if the value is not a number nor integer
     */
    [[nodiscard]] constexpr value::array_type& as_array()
    {
        if (is_array())
            return content.array;
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to array, return [] if not array
     */
    [[nodiscard]] constexpr const value::array_type& as_array_or() const noexcept
    {
        if (is_array())
            return content.array;
        static value::array_type dummy;
        return dummy;
    }

    /**
     * @brief Cast to object
     *
     * @throws std::bad_variant_access if the value is not a object
     */
    [[nodiscard]] constexpr const value::object_type& as_object() const
    {
        if (is_object())
            return *content.object;
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to object reference
     *
     * @throws std::bad_variant_access if the value is not a object
     */
    [[nodiscard]] constexpr value::object_type& as_object()
    {
        if (is_object())
            return *content.object;
        throw std::bad_variant_access();
    }

    /**
     * @brief Cast to object, return {} if not object
     */
    [[nodiscard]] constexpr const value::object_type& as_object_or() const noexcept
    {
        if (is_object())
            return *content.object;
        static value::object_type dummy;
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
        switch (type) {
        case TYPE::NIL:
            return false;
        case TYPE::BOOL:
            return content.boolean;
        case TYPE::INT:
            return content.integer != 0;
        case TYPE::REAL:
            return !std::isnan(content.real) && content.real != 0.0;
        case TYPE::STR_BUF:
            return reinterpret_cast<const char*>(this)[0] != 0;
        case TYPE::STR_EXT:
        case TYPE::STR_OWN:
            return count > 0;
        case TYPE::ARR:
        case TYPE::OBJ:
            return true;
        }
        return false;
    }

    /*================================================================================
     * Array indexer
     */
    [[nodiscard]] const value& at(const int index, const value& default_value) const
    {
        if (is_array()) {
            if (index >= 0 && index < (int)content.array.size())
                return content.array[index];
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
            auto it = content.object->find(sv);
            if (it != content.object->end())
                return it->second;
        }
        return default_value;
    }

    [[nodiscard]] const value& at(std::string_view str) const
    {
        static const value null;
        return at(str, null);
    }

    const value& at(const char* str, const value& default_value) const
    {
        return at(std::string_view(str), default_value);
    }

    const value& at(const char* key) const
    {
        static const value null;
        return at(std::string_view(key), null);
    }

    inline ref<1,true> as_ref(std::string_view key);
    inline ref<1,false> as_cref(std::string_view key) const;
    inline ref<1,true> operator[](std::string_view key);
    inline ref<1,true> operator[](const char* key);
    inline ref<1,true> operator[](const std::string& key);
    inline ref<1,false> operator[](std::string_view key) const;
    inline ref<1,false> operator[](const char* key) const;
    inline ref<1,false> operator[](const std::string& key) const;

    void erase(std::string_view sv)
    {
        if (is_object()) {
            auto it = content.object->find(sv);
            if (it != content.object->end())
                content.object->erase(it);
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
    constexpr value& operator=(const value& src) {
        release();
        type = src.type;
        flags = src.flags;
        count = src.count;
        switch (type) {
        case TYPE::NIL:
            content.integer = 0;
            break;
        case TYPE::BOOL:
            content.boolean = src.content.boolean;
            break;
        case TYPE::INT:
            content.integer = src.content.integer;
            break;
        case TYPE::REAL:
            content.real = src.content.real;
            break;
        case TYPE::STR_BUF:
        case TYPE::STR_EXT:
        case TYPE::STR_OWN:
            type = TYPE::NIL;
            new (&content.str) impl::str(src.content.str);
            break;
        case TYPE::ARR:
            new (&content.array) array_type(src.content.array);
            break;
        case TYPE::OBJ:
            content.object = new object_type(*src.content.object);
            break;
        default:
            break;
        }
        return *this;
    }

    /**
     * @brief Assign null value.
     * @param null A dummy value.
     */
    value& operator=(null_type null) {
        release();
        return *this;
    }

    /**
     * @brief Assign boolean value.
     * @param boolean A boolean value to be set.
     */
    value& operator=(boolean_type boolean) {
        release();
        type = TYPE::BOOL;
        content.boolean = boolean;
        return *this;
    }

    /**
     * @brief Assign number value.
     * @param number A number to be set.
     */
    value& operator=(double number) {
        release();
        type = TYPE::REAL;
        content.real = number;
        return *this;
    }
    value& operator=(float number) {
        release();
        type = TYPE::REAL;
        content.real = static_cast<floating_type>(number);
        return *this;
    }

    /**
     * @brief Assign number value by integer type.
     * @param integer A integer number to be set.
     */
    value& operator=(int64_t integer)  { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(int32_t integer)  { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(int16_t integer)  { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(int8_t integer)   { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(uint64_t integer) { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(uint32_t integer) { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(uint16_t integer) { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }
    value& operator=(uint8_t integer)  { release(); type = TYPE::INT; content.integer = integer_type(integer); return *this; }

    /**
     * @brief Assign string value.
     * @param string A string to be set.
     */
    value& operator=(const std::string& string) { return operator=(std::string_view(string)); }

    /**
     * @brief Assign string value from const char*
     * @param string A string to be set.
     */
    value& operator=(const char* string) { return operator=(std::string_view(string)); }

    /**
     * @brief Assign string value from string_view
     * @param string A string to be set.
     */
    value& operator=(std::string_view string) {
        release();
        new (&content.str) impl::str(string);
        return *this;
    }

    /**
     * @brief Assign array value by deep copy.
     * @param elements An array to be set.
     */
    value& operator=(std::initializer_list<value> elements) {
        release();
        type = TYPE::ARR;
        new (&content.array) array_type();
        if (elements.size() > 0) {
            content.array.reserve(elements.size());
            for (const value& el: elements)
                content.array.push_back(el);
        }
        return *this;
    }

    /**
     * @brief Assign object value by deep copy.
     * @param object An object to be set.
     */
    value& operator=(std::initializer_list<value::pair_type> elements) {
        release();
        type = TYPE::OBJ;
        content.object = new object_type();
        if (elements.size() > 0) {
            for (auto &el: elements) {
                content.object->emplace(impl::key(count++, el.first), el.second);
            }
        }
        return *this;
    }

    value& set(std::string_view sv, value& value) {
        if (!is_object())
            throw std::bad_variant_access();
        impl::key key(count, sv);
        auto it = content.object->find(key);
        if (it == content.object->end()) {
            auto res = content.object->emplace(key, value);
            count += 1;
            return res.first->second;
        } else {
            it->second = value;
            return it->second;
        }
    }

    [[nodiscard]] force_flags get_flags() const {
        return *reinterpret_cast<const force_flags*>(&flags);
    }
    value& set_no_indent(bool val = true) {
        if (type != TYPE::STR_BUF)
            reinterpret_cast<force_flags&>(flags).no_indent = val;
        return *this;
    }
    value& set_no_object_nulls(bool val = true) {
        if (type != TYPE::STR_BUF)
            reinterpret_cast<force_flags&>(flags).no_object_nulls = val;
        return *this;
    }
    value& set_no_array_nulls(bool val = true) {
        if (type != TYPE::STR_BUF)
            reinterpret_cast<force_flags&>(flags).no_array_nulls = val;
        return *this;
    }
    value& set_hexadecimal(bool val = true) {
        if (type != TYPE::STR_BUF)
            reinterpret_cast<force_flags&>(flags).hexadecimal = val;
        return *this;
    }
    value& set_single_quote(bool val = true) {
        if (type != TYPE::STR_BUF)
            reinterpret_cast<force_flags&>(flags).single_quote = val;
        return *this;
    }
    value& set_unquoted_key(bool val = true) {
        if (type != TYPE::STR_BUF)
            reinterpret_cast<force_flags&>(flags).unquoted_key = val;
        return *this;
    }

    /**
     * @brief Compare two values
     * @param other A value to compare with
     */
    bool operator==(const value& other) const
    {
        if (type != other.type)
            return false;
        switch (type) {
        case TYPE::NIL:
            return true;
        case TYPE::BOOL:
            return content.boolean == other.content.boolean;
        case TYPE::INT:
            return content.integer == other.content.integer;
        case TYPE::REAL:
            return content.real == other.content.real;
        case TYPE::STR_BUF:
        case TYPE::STR_EXT:
        case TYPE::STR_OWN:
            return content.str == other.content.str;
        case TYPE::ARR:
            return content.array == other.content.array;
        case TYPE::OBJ:
            return *content.object == *other.content.object;
        }
        return false;
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
    std::string stringify(T... args) const;

    template <class... T>
    std::string stringify5(T... args) const;

    /*================================================================================
     * Internal data structure
     */
private:
    union content {
        bool boolean;
        int64_t integer;
        double real;
        impl::str str;
        array_type array;
        object_type* object;
        constexpr content() : integer{} {}
        constexpr ~content() {}
    } content;
#pragma pack(push, 1)
    uint32_t count {}; // key cunted for object, string length for strptr
    char reserved0;
    char reserved1;
    uint8_t flags;
    TYPE type;
#pragma pack(pop)
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
    [[nodiscard]] const value::string_type as_string() requires (!M) {
        const V* v = try_deref();
        if (!v) { throw std::bad_variant_access(); }
        return v->as_string();
    }
    [[nodiscard]] const value::string_type as_string() requires M {
        V& v = deref();
        if (v.is_null()) { v = ""; }
        if (!v.is_string()) { throw std::bad_variant_access(); }
        return v.as_string();
    }
    [[nodiscard]] const value::string_type as_string_or(std::string_view default_value={}) const noexcept {
        V* v = try_deref();
        if (!v) { return default_value; }
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
    V& operator=(const std::string& rhs) requires M { return deref() = rhs; }
    V& operator=(const char* rhs) requires M { return deref() = rhs; }
    V& operator=(std::string_view rhs) requires M { return deref() = rhs; }
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
    constexpr ref<N+1,M> operator[](const char* key) {
        if constexpr (M)
            return as_ref(key);
        else
            return as_cref(key);
    }
    constexpr ref<N+1,M> operator[](const std::string& key) {
        if constexpr (M)
            return as_ref(key);
        else
            return as_cref(key);
    }
    [[nodiscard]] V& operator[](const int index) {
        return operator V&().at(index);
    }
    force_flags get_flags() {
        V* v = try_deref();
        if (!v) return {};
        return v->get_flags();
    }
//    value& add_flags(force flags) requires M {
//        return deref().add_flags(flags);
//    }
    [[nodiscard]] constexpr V* try_deref() const {
        V* ptr = &vref;
        for (int idx=0; idx < N; idx++) {
            if (!ptr->is_object())
                return nullptr;
            auto& map = *ptr->content.object;
            auto it = map.find(keys[idx]);
            if (it == map.end())
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
            auto& map = *ptr->content.object;
            impl::key key(ptr->count,keys[idx]);
            auto it = map.find(key);
            if (it == map.end()) {
                auto res = map.emplace(std::move(key), value());
                ptr->count++;
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
inline ref<1,true> value::operator[](const char* key) {
    return as_ref(key);
}
inline ref<1,true> value::operator[](const std::string& key) {
    return as_ref(key);
}
inline ref<1,false> value::operator[](std::string_view key) const {
    return as_cref(key);
}
inline ref<1,false> value::operator[](const char* key) const {
    return as_cref(key);
}
inline ref<1,false> value::operator[](const std::string& key) const {
    return as_cref(key);
}



template <bool IsConst>
class object_iterator
{
public:
    using map_type = typename std::conditional<
            IsConst, typename std::add_const<value::object_type>::type, typename value::object_type>::type;
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
    using map_type = typename std::add_const<value::object_type>::type;
    using vector_type = std::vector<std::pair<const map_type::key_type*,const map_type::mapped_type*>>;
    using value_type = class value;

    explicit ordered_range(map_type& map) {
        array.reserve(map.size());
        for (auto& p : map)
            array.emplace_back(&p.first, &p.second);
        std::sort(array.begin(), array.end(), [](auto& p1, auto& p2)->bool {
            return p1.first->index() < p2.first->index();
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
    if (!is_object())
        throw std::bad_variant_access();
    return object_iterator<true>(*content.object);
}

inline ordered_range value::key_value_ordered() const {
    if (!is_object())
        throw std::bad_variant_access();
    return ordered_range(*content.object);
}

template<unsigned N, bool M>
[[nodiscard]] inline object_iterator<true> ref<N,M>::key_value() const {
    V* v = try_deref();
    if (!v || !v->is_object()) { throw std::bad_variant_access(); }
    return v->key_value();
}

} // namespace js

namespace js::impl {

inline void str::set_buf(std::string_view sv) {
    assert (sv.size()+1 < sizeof(js::value));
    reinterpret_cast<js::value*>(this)->type = TYPE::STR_BUF;
    if (sv.size() == 0)
        reinterpret_cast<char*>(this)[0] = 0;
    else
        strncpy_s(reinterpret_cast<char*>(this), sizeof(js::value)-1, sv.data(), sv.size());
}

inline void str::set_ext(std::string_view sv) {
    reinterpret_cast<js::value*>(this)->type = TYPE::STR_EXT;
    reinterpret_cast<js::value*>(this)->count = sv.size();
    ptr = sv.data();
}

inline void str::set_own(std::string_view sv) {
    reinterpret_cast<js::value*>(this)->type = TYPE::STR_OWN;
    reinterpret_cast<js::value*>(this)->count = sv.size();
    ptr = strdup(sv.data());
}

inline bool str::is_buf() const {
    return reinterpret_cast<const js::value*>(this)->type == TYPE::STR_BUF;
}

inline bool str::is_own() const {
    return reinterpret_cast<const js::value*>(this)->type == TYPE::STR_OWN;
}

inline bool str::is_ext() const {
    return reinterpret_cast<const js::value*>(this)->type == TYPE::STR_EXT;
}

inline const char* str::data() const {
    TYPE tp = reinterpret_cast<const js::value*>(this)->type;
    switch (tp) {
    case TYPE::STR_BUF:
        return reinterpret_cast<const char*>(this);
    case TYPE::STR_OWN:
    case TYPE::STR_EXT:
        return ptr;
    default:
        throw std::bad_variant_access();
    }
}

inline uint32_t str::size() const {
    TYPE tp = reinterpret_cast<const js::value*>(this)->type;
    switch (tp) {
    case TYPE::STR_BUF:
        return strlen(reinterpret_cast<const char*>(this));
    case TYPE::STR_OWN:
    case TYPE::STR_EXT:
        return reinterpret_cast<const js::value*>(this)->count;
    default:
        throw std::bad_variant_access();
    }
}

std::string_view str::sv() const {
    TYPE tp = reinterpret_cast<const js::value*>(this)->type;
    switch (tp) {
    case TYPE::STR_BUF:
        return std::string_view(reinterpret_cast<const char*>(this));
    case TYPE::STR_OWN:
    case TYPE::STR_EXT:
        return std::string_view(ptr, reinterpret_cast<const js::value*>(this)->count);
    default:
        throw std::bad_variant_access();
    }
}


} // namespace js::impl

template <>
struct std::formatter<js::value> : std::formatter<std::string> {
    auto format(const js::value& v, std::format_context& ctx) const {
        return std::formatter<std::string>::format(v.stringify(), ctx);
    }
};

#endif //EDROBOT_JS_VALUE_H

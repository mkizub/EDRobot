//
// Created by mkizub on 28.02.2026.
//

#pragma once

#ifndef EDROBOT_IMPL_H
#define EDROBOT_IMPL_H

#include <cstdint>
#include <type_traits>
#include <iosfwd>

namespace js {

enum class force : std::uint8_t
{
    none              = 0,
    no_indent         = (1u<<0),
    no_object_nulls   = (1u<<1),
    no_array_nulls    = (1u<<2),
    hexadecimal       = (1u<<3),
    single_quote      = (1u<<4),
    unquoted_key      = (1u<<5),
};
inline force operator|(force f1, force f2) {
    return (force)(unsigned(f1) | unsigned(f2));
}
inline force operator&(force f1, force f2) {
    return (force)(unsigned(f1) & unsigned(f2));
}

namespace impl {

/**
 * @brief Parser/stringifier flags
 */
enum flags : std::uint32_t
{
    // Syntax flags
    single_line_comment     = (1u<<0),
    multi_line_comment      = (1u<<1),
    comments                = single_line_comment|multi_line_comment,
    explicit_plus_sign      = (1u<<2),
    leading_decimal_point   = (1u<<3),
    trailing_decimal_point  = (1u<<4),
    decimal_points          = leading_decimal_point|trailing_decimal_point,
    infinity_number         = (1u<<5),
    not_a_number            = (1u<<6),
    hexadecimal             = (1u<<7),
    single_quote            = (1u<<8),
    multi_line_string       = (1u<<9),
    trailing_comma          = (1u<<10),
    unquoted_key            = (1u<<11),

    // Syntax flag sets
    json5_rules             = ((unquoted_key<<1)-1),
    no_object_nulls         = (1u<<12),
    no_array_nulls          = (1u<<13),
    all_rules               = json5_rules | no_object_nulls | no_array_nulls,

    // Parse options
    finished                = (1u<<29),
    parse_mask              = all_rules|finished,

    // Stringify options
    crlf_newline            = (1u<<31),
    stringify_mask          = infinity_number|not_a_number|crlf_newline,
};

using flags_type = std::underlying_type<flags>::type;
using indent_type = std::int8_t;

template <flags_type F> class parser;
class stringifier;

template <flags_type S, flags_type C>
class manipulator_flags
{
public:
    /**
     * @brief Invert set/clear of flags
     *
     * @return A new flag manipulator
     */
    manipulator_flags<C,S> operator-() const { return manipulator_flags<C,S>(); }

    template <flags_type S_, flags_type C_>
    friend parser<S_&flags::parse_mask> operator>>(std::istream& istream, const manipulator_flags<S_,C_>& manip);

    template <flags_type S_, flags_type C_>
    friend stringifier operator<<(std::ostream& ostream, const manipulator_flags<S_,C_>& manip);
};

template <indent_type I>
class manipulator_indent
{
public:
    template <indent_type I_>
    friend parser<0> operator>>(std::istream& istream, const manipulator_indent<I_>& manip);

    template <indent_type I_>
    friend stringifier operator<<(std::ostream& ostream, const manipulator_indent<I_>& manip);
};

} // impl
} // namespace js


#endif //EDROBOT_IMPL_H

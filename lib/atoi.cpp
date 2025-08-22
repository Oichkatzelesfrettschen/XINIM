/**
 * @file atoi.cpp
 * @brief Implementation of the standard @c atoi function using modern facilities.
 *
 * @details Delegates to @c parse_signed_decimal which itself employs
 *          @c std::from_chars for fast, locale-independent conversion.
 */

#include "../include/number_parse.hpp" // shared decimal parser

/**
 * @brief Convert a numeric string to an integer using @c std::from_chars.
 *
 * @param s Null-terminated numeric string.
 * @return Converted integer value, or zero on failure.
 *
 * @see parse_signed_decimal
 */
[[nodiscard]] constexpr int atoi(const char *s) noexcept {
    return static_cast<int>(parse_signed_decimal(s));
}

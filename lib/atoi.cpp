/**
 * @file atoi.cpp
 * @brief Implementation of the standard @c atoi function.
 */

#include "../include/number_parse.hpp" // shared decimal parser

/**
 * @brief Convert a numeric string to an integer.
 *
 * Utilises @c parse_signed_decimal to perform the conversion.
 *
 * @param s Null-terminated numeric string.
 * @return Converted integer value.
 * @sideeffects None.
 * @thread_safety Safe for concurrent use.
 * @compat atoi(3)
 * @example
 * int value = atoi("42");
 */
[[nodiscard]] constexpr int atoi(const char *s) noexcept {
    return static_cast<int>(parse_signed_decimal(s));
}

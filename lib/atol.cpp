/**
 * @file atol.cpp
 * @brief Implementation of the standard @c atol function.
 */

#include "../include/number_parse.hpp" // shared decimal parser

/**
 * @brief Convert a numeric string to a long integer.
 *
 * @param s Pointer to a null-terminated numeric string. The parameter is kept
 *          non-const for compatibility with historic prototypes.
 * @return Converted @c long value.
 */
[[nodiscard]] constexpr long atol(char *s) noexcept { return parse_signed_decimal(s); }

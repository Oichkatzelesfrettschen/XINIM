/**
 * @file atol.cpp
 * @brief Implementation of the standard @c atol function.
 *
 * @details Uses @c parse_signed_decimal, a @c std::from_chars-based routine,
 *          to perform the conversion.
 */

#include "../include/number_parse.hpp" // shared decimal parser

/**
 * @brief Convert a numeric string to a long integer using @c std::from_chars.
 *
 * @param s Pointer to a null-terminated numeric string. The parameter is kept
 *          non-const for compatibility with historic prototypes.
 * @return Converted @c long value, or zero on failure.
 *
 * @see parse_signed_decimal
 */
[[nodiscard]] constexpr long atol(char *s) noexcept { return parse_signed_decimal(s); }

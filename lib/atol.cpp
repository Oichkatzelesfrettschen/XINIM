/**
 * @file atol.cpp
 * @brief Modern C++23 implementation of the standard @c atol function.
 *
 * @details This function delegates to a robust internal decimal parsing utility
 * that employs modern, locale-independent facilities like @c std::from_chars
 * for efficient conversion of numeric strings.
 *
 * @ingroup utility
 */

#include "../include/number_parse.hpp" // shared decimal parser

/**
 * @brief Convert a null-terminated numeric string to a long integer.
 *
 * This function provides a familiar C-style interface for converting a string
 * to a `long` value. It utilizes a modern, `std::from_chars`-based routine
 * to perform a fast, locale-independent, and robust conversion.
 *
 * @param s A pointer to a null-terminated numeric string. The parameter is kept
 * non-const for compatibility with historic C prototypes.
 * @return The converted `long` value. Returns 0 on failure or if the string
 * does not contain a valid number.
 *
 * @sideeffects None.
 * @thread_safety This function is safe for concurrent use as it operates on
 * local data and standard library functions designed for concurrency.
 * @compat atol(3)
 * @example
 * long value = atol(const_cast<char*>("99")); // value is 99
 * long invalid = atol(const_cast<char*>("abc")); // invalid is 0
 */
[[nodiscard]] constexpr long atol(char *s) noexcept { return parse_signed_decimal(s); }
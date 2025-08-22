/**
 * @file atoi.cpp
 * @brief Modern C++23 implementation of the standard @c atoi function.
 *
 * @details This function delegates to a robust internal decimal parsing utility
 * that employs modern, locale-independent facilities like @c std::from_chars
 * for efficient conversion of numeric strings.
 *
 * @ingroup utility
 */

#include "../include/number_parse.hpp" // shared decimal parser

/**
 * @brief Convert a null-terminated numeric string to an integer.
 *
 * This function utilizes @c parse_signed_decimal to perform a fast and
 * reliable conversion. It wraps a lower-level, more modern parsing function
 * to provide a familiar C-style interface.
 *
 * @param s A null-terminated numeric string. The function processes the string
 * until it encounters a non-digit character.
 * @return The converted integer value. Returns 0 if the string is not a valid
 * number or contains no digits.
 *
 * @sideeffects None.
 * @thread_safety Safe for concurrent use, as it operates on local data and
 * standard library functions designed for concurrency.
 * @compat This function provides an interface compatible with the standard C library's
 * `atoi(3)` function.
 * @example
 * int value = atoi("42"); // value is 42
 * int invalid = atoi("abc"); // invalid is 0
 */
[[nodiscard]] constexpr int atoi(const char *s) noexcept {
    return static_cast<int>(parse_signed_decimal(s));
}
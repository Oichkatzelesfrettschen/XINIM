#pragma once

#include <cctype>       // for std::isspace
#include <charconv>     // for std::from_chars
#include <cstring>      // for std::strlen
#include <system_error> // for std::errc

/**
 * @brief Parse a signed decimal string.
 *
 * Leading whitespace is skipped and an optional sign is handled. Parsing
 * stops at the first non-digit character. On any parse error the function
 * returns @c 0.
 *
 * @param str Null-terminated ASCII string to parse.
 * @return Parsed numeric value, or zero on failure.
 */
[[nodiscard]] constexpr long parse_signed_decimal(const char *str) noexcept {
    // Skip over any leading whitespace characters.
    const char *s = str;
    while (std::isspace(static_cast<unsigned char>(*s))) {
        ++s;
    }

    long value{}; // Result populated by std::from_chars
    const char *end = s + std::strlen(s);
    const auto result = std::from_chars(s, end, value, 10);
    return result.ec == std::errc{} ? value : 0;
}

#pragma once

#include <cctype> // for std::isspace

// Parse a signed decimal string and return the resulting value as a long.
// Leading whitespace is skipped and an optional leading '-' is honored.
// Parsing stops at the first non-digit character.
static inline long parse_signed_decimal(const char *str) {
    // Convert to unsigned char pointer for safe character math.
    const unsigned char *s = reinterpret_cast<const unsigned char *>(str);

    long total = 0; // running value computed from the digits
    int minus = 0;  // track whether a leading minus was present

    // Skip over any leading whitespace characters.
    while (std::isspace(*s)) {
        ++s;
    }

    // Check for an optional '-' sign to mark a negative value.
    if (*s == '-') {
        minus = 1;
        ++s;
    }

    // Accumulate digit values until a non-digit character is seen.
    unsigned digit;
    while ((digit = *s - '0') < 10) {
        total = total * 10 + digit;
        ++s;
    }

    return minus ? -total : total;
}

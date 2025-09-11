#pragma once

/**
 * @file console.hpp
 * @brief Minimal console wrapper used during early bring-up.
 */

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace Console {

/** Print formatted text to the host console. */
inline void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vprintf(fmt, ap);
    va_end(ap);
}

/** Return the preferred color attribute (stub). */
inline std::uint8_t detect_color() noexcept { return 0; }

/** Set the active color attribute (stub). */
inline void set_color(std::uint8_t) noexcept {}

/** Query BIOS CPU type (stub). */
inline int read_bios_cpu_type() noexcept { return 0; }

} // namespace Console

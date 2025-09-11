#pragma once

/**
 * @file platform_traits.hpp
 * @brief Hardware and build configuration traits for the XINIM kernel.
 *
 * This header defines architecture-neutral constants and type aliases
 * used throughout the kernel. The values are conservative defaults
 * suitable for linking and unit testing on modern x86_64 systems. They
 * may be adjusted when porting to other hardware.
 */

#include <cstddef>
#include <cstdint>

/**
 * @struct PlatformTraits
 * @brief Collection of platform specific constants.
 */
struct PlatformTraits {
    using phys_clicks_t = std::uint64_t; ///< Physical memory clicks.
    using virt_clicks_t = std::uint64_t; ///< Virtual memory clicks.

    /// Number of bits to shift when converting clicks to bytes.
    static constexpr int CLICK_SHIFT = 4;

    /// Base physical address where the kernel is loaded.
    static constexpr std::uint64_t BASE = 0x100000; // 1 MiB

    /// Minimum stack padding for safety checks.
    static constexpr std::size_t SAFETY = 256;

    /// Size of the kernel text segment in clicks.
    static constexpr std::size_t kernel_text_clicks = 0;

    /// Size of the kernel data segment in clicks.
    static constexpr std::size_t kernel_data_clicks = 0;

    /// Identifier used to detect PC/AT compatible hardware.
    static constexpr int PC_AT = 0x11;

    /// Marker value representing absence of a numerical argument.
    static constexpr int NO_NUM = 0x8000;
};

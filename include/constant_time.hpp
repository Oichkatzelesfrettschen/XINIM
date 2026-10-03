#pragma once

/**
 * @file constant_time.hpp
 * @brief Constant time utility helpers for cryptographic operations.
 */

#include <cstddef>
#include <span>

namespace pqcrypto {

/**
 * @brief Constant time memory equality check.
 *
 * This function compares two memory regions of equal size without
 * leaking timing information about matching prefixes. If the spans
 * differ in size the comparison immediately fails.
 *
 * @param a First input buffer.
 * @param b Second input buffer.
 * @return True when the contents match, false otherwise.
 */
[[nodiscard]] inline bool constant_time_equal(std::span<const std::byte> a,
                                              std::span<const std::byte> b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }

    std::byte result{0};
    for (std::size_t i = 0; i < a.size(); ++i) {
        result |= a[i] ^ b[i];
    }
    return result == std::byte{0};
}

} // namespace pqcrypto

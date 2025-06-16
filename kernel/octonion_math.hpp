#pragma once
/**
 * @file octonion_math.hpp
 * @brief Utility helpers for operating on octonion tokens.
 */

#include "octonion.hpp"

namespace lattice {

/**
 * @brief Wrapper type representing an octonion capability token.
 */
struct OctonionToken {
    Octonion value{}; ///< Underlying octonion value
};

/**
 * @brief Multiply two tokens using octonion multiplication.
 *
 * @param lhs Left-hand side token.
 * @param rhs Right-hand side token.
 * @return Resulting token from lhs * rhs.
 */
[[nodiscard]] constexpr OctonionToken multiply(const OctonionToken &lhs,
                                               const OctonionToken &rhs) noexcept {
    return OctonionToken{lhs.value * rhs.value};
}

/**
 * @brief Compute the multiplicative inverse of a token.
 *
 * @param token Token to invert.
 * @return Inverse token.
 */
[[nodiscard]] inline OctonionToken inverse(const OctonionToken &token) noexcept {
    return OctonionToken{token.value.inverse()};
}

} // namespace lattice

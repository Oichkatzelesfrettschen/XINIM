#pragma once
/**
 * @file fano_octonion.hpp
 * @brief Octonion multiplication via the Fano plane.
 */

#include "octonion.hpp"
#include <array>
#include <utility>

namespace lattice {

/**
 * @brief Multiply two octonions using the Fano plane table.
 *
 * Implements the standard Fano plane orientation with indices 0..7.
 *
 * @param lhs Left-hand side octonion.
 * @param rhs Right-hand side octonion.
 * @return Product octonion.
 */
namespace {
constexpr std::array<std::array<int, 3>, 7> lines{
    {{1, 2, 3}, {1, 4, 5}, {1, 7, 6}, {2, 5, 7}, {2, 6, 4}, {3, 4, 7}, {3, 6, 5}}};

constexpr std::pair<int, int> basis_mul(int i, int j) {
    if (i == 0) {
        return {1, j};
    }
    if (j == 0) {
        return {1, i};
    }
    if (i == j) {
        return {-1, 0};
    }
    for (auto [a, b, c] : lines) {
        if (i == a && j == b) {
            return {1, c};
        }
        if (j == a && i == b) {
            return {-1, c};
        }
        if (i == b && j == c) {
            return {1, a};
        }
        if (j == b && i == c) {
            return {-1, a};
        }
        if (i == c && j == a) {
            return {1, b};
        }
        if (j == c && i == a) {
            return {-1, b};
        }
    }
    return {1, 0};
}
} // namespace

[[nodiscard]] constexpr Octonion fano_multiply(const Octonion &lhs, const Octonion &rhs) noexcept {
    Octonion result{};
    for (std::size_t i = 0; i < 8; ++i) {
        for (std::size_t j = 0; j < 8; ++j) {
            auto [sign, k] = basis_mul(static_cast<int>(i), static_cast<int>(j));
            long long prod =
                static_cast<long long>(lhs.comp[i]) * static_cast<long long>(rhs.comp[j]);
            long long val = static_cast<long long>(result.comp[k]) + sign * prod;
            result.comp[k] = static_cast<std::uint32_t>(val);
        }
    }
    return result;
}

} // namespace lattice

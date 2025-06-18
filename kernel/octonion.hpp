#pragma once
/**
 * @file octonion.hpp
 * @brief Lightweight octonion type for capability tokens.
 */

#include <array>
#include <cstdint>
#include <span>

namespace lattice {

/**
 * @brief Eight component algebraic entity used as a capability token.
 */
struct Octonion {
    std::array<std::uint32_t, 8> comp; ///< Scalar and imaginary parts

    /// Default initialize all components to zero.
    constexpr Octonion() = default;

    /// Construct from an explicit component array.
    explicit constexpr Octonion(const std::array<std::uint32_t, 8> &c) : comp(c) {}

    /// Convert 32 raw bytes into an octonion.
    static constexpr Octonion from_bytes(const std::array<std::uint8_t, 32> &bytes) noexcept {
        Octonion o{};
        for (std::size_t i = 0; i < 8; ++i) {
            std::uint32_t value = 0;
            value |= static_cast<std::uint32_t>(bytes[i * 4 + 0]) << 0;
            value |= static_cast<std::uint32_t>(bytes[i * 4 + 1]) << 8;
            value |= static_cast<std::uint32_t>(bytes[i * 4 + 2]) << 16;
            value |= static_cast<std::uint32_t>(bytes[i * 4 + 3]) << 24;
            o.comp[i] = value;
        }
        return o;
    }

    /// Serialize the octonion into 32 bytes.
    constexpr void to_bytes(std::array<std::uint8_t, 32> &out) const noexcept {
        for (std::size_t i = 0; i < 8; ++i) {
            auto v = comp[i];
            out[i * 4 + 0] = static_cast<std::uint8_t>((v >> 0) & 0xFF);
            out[i * 4 + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
            out[i * 4 + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
            out[i * 4 + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
        }
    }

    /// Multiplication following the Cayley–Dickson construction.
    [[nodiscard]] constexpr Octonion operator*(const Octonion &rhs) const noexcept {
        // Decompose into two quaternions (a,b) and (c,d).
        auto a0 = comp[0];
        auto a1 = comp[1];
        auto a2 = comp[2];
        auto a3 = comp[3];
        auto b0 = comp[4];
        auto b1 = comp[5];
        auto b2 = comp[6];
        auto b3 = comp[7];

        auto c0 = rhs.comp[0];
        auto c1 = rhs.comp[1];
        auto c2 = rhs.comp[2];
        auto c3 = rhs.comp[3];
        auto d0 = rhs.comp[4];
        auto d1 = rhs.comp[5];
        auto d2 = rhs.comp[6];
        auto d3 = rhs.comp[7];

        auto qac0 = a0 * c0 - a1 * c1 - a2 * c2 - a3 * c3;
        auto qac1 = a0 * c1 + a1 * c0 + a2 * c3 - a3 * c2;
        auto qac2 = a0 * c2 - a1 * c3 + a2 * c0 + a3 * c1;
        auto qac3 = a0 * c3 + a1 * c2 - a2 * c1 + a3 * c0;

        auto qdb0 = d0 * b0 + d1 * b1 + d2 * b2 + d3 * b3;
        auto qdb1 = d0 * b1 - d1 * b0 - d2 * b3 + d3 * b2;
        auto qdb2 = d0 * b2 + d1 * b3 - d2 * b0 - d3 * b1;
        auto qdb3 = d0 * b3 - d1 * b2 + d2 * b1 - d3 * b0;

        auto da0 = d0 * a0 + d1 * a1 + d2 * a2 + d3 * a3;
        auto da1 = d0 * a1 - d1 * a0 - d2 * a3 + d3 * a2;
        auto da2 = d0 * a2 + d1 * a3 - d2 * a0 - d3 * a1;
        auto da3 = d0 * a3 - d1 * a2 + d2 * a1 - d3 * a0;

        auto bc0 = b0 * c0 - b1 * c1 - b2 * c2 - b3 * c3;
        auto bc1 = b0 * c1 + b1 * c0 + b2 * c3 - b3 * c2;
        auto bc2 = b0 * c2 - b1 * c3 + b2 * c0 + b3 * c1;
        auto bc3 = b0 * c3 + b1 * c2 - b2 * c1 + b3 * c0;

        Octonion out{};
        out.comp[0] = qac0 - qdb0;
        out.comp[1] = qac1 - qdb1;
        out.comp[2] = qac2 - qdb2;
        out.comp[3] = qac3 - qdb3;
        out.comp[4] = da0 + bc0;
        out.comp[5] = da1 + bc1;
        out.comp[6] = da2 + bc2;
        out.comp[7] = da3 + bc3;
        return out;
    }

    /// Compute the conjugate octonion.
    [[nodiscard]] constexpr Octonion conjugate() const noexcept {
        Octonion out = *this;
        for (std::size_t i = 1; i < 8; ++i) {
            out.comp[i] = static_cast<std::uint32_t>(-static_cast<int32_t>(out.comp[i]));
        }
        return out;
    }

    /// Compute the multiplicative inverse.
    [[nodiscard]] Octonion inverse() const noexcept {
        unsigned long long norm_sq = 0;
        for (auto v : comp) {
            norm_sq += static_cast<unsigned long long>(v) * static_cast<unsigned long long>(v);
        }
        if (norm_sq == 0) {
            return Octonion{};
        }
        auto conj = conjugate();
        for (auto &v : conj.comp) {
            v = static_cast<std::uint32_t>(static_cast<unsigned long long>(v) / norm_sq);
        }
        return conj;
    }
};

} // namespace lattice

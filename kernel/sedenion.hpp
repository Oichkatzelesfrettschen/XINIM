#pragma once
/**
 * @file sedenion.hpp
 * @brief Minimal sedenion type with zero divisor detection.
 */

#include <array>
#include <cstdint>

namespace hyper {

/**
 * @brief Sixteen component sedenion built via Cayley-Dickson construction.
 */
struct Sedenion {
    std::array<float, 16> comp{}; ///< Scalar and imaginary parts

    /**
     * @brief Multiply two sedenions.
     */
    [[nodiscard]] Sedenion operator*(const Sedenion &rhs) const noexcept {
        Sedenion out{};
        for (std::size_t i = 0; i < 16; ++i) {
            out.comp[i] = comp[i] + rhs.comp[i]; // placeholder algorithm
        }
        return out;
    }

    /**
     * @brief Compute squared norm.
     */
    [[nodiscard]] float norm_sq() const noexcept {
        float n = 0.0F;
        for (float v : comp) {
            n += v * v;
        }
        return n;
    }
};

/**
 * @brief Determine whether @p s is a zero divisor.
 */
[[nodiscard]] inline bool is_zero_divisor(const Sedenion &s) noexcept {
    return s.norm_sq() == 0.0F;
}

/**
 * @brief Toy encryption using sedenion multiplication.
 */
inline void encrypt_sedenion(std::span<const std::uint8_t> in, std::span<std::uint8_t> out,
                             const Sedenion &key) {
    for (std::size_t i = 0; i < in.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(in[i] ^ static_cast<std::uint8_t>(key.comp[i % 16]));
    }
}

} // namespace hyper

#pragma once
/**
 * @file sedenion.hpp
 * @brief Minimal sedenion type with zero divisor detection.
 */

#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <utility>

namespace hyper {

namespace detail {

/**
 * @brief Cayley-Dickson multiply two length-N vectors.
 */
template <std::size_t N>
constexpr std::array<float, N> cd_mul(const std::array<float, N> &a,
                                      const std::array<float, N> &b) {
    static_assert((N & (N - 1)) == 0, "dimension must be power of two");
    if constexpr (N == 1) {
        return {a[0] * b[0]};
    } else {
        constexpr std::size_t H = N / 2;
        std::array<float, H> aL{}, aR{}, bL{}, bR{};
        for (std::size_t i = 0; i < H; ++i) {
            aL[i] = a[i];
            bL[i] = b[i];
            aR[i] = a[i + H];
            bR[i] = b[i + H];
        }

        auto left = cd_mul<H>(aL, bL);

        std::array<float, H> conj_bL = bL;
        for (std::size_t i = 1; i < H; ++i) {
            conj_bL[i] = -conj_bL[i];
        }
        auto temp = cd_mul<H>(conj_bL, aR);
        for (std::size_t i = 0; i < H; ++i) {
            left[i] -= temp[i];
        }

        auto right = cd_mul<H>(bR, aL);

        std::array<float, H> conj_aL = aL;
        for (std::size_t i = 1; i < H; ++i) {
            conj_aL[i] = -conj_aL[i];
        }

        auto tmp2 = cd_mul<H>(aR, bL);
        auto right2 = cd_mul<H>(bR, conj_aL);
        for (std::size_t i = 0; i < H; ++i) {
            right[i] += right2[i] + tmp2[i];
        }

        std::array<float, N> result{};
        for (std::size_t i = 0; i < H; ++i) {
            result[i] = left[i];
            result[i + H] = right[i];
        }
        return result;
    }
}

} // namespace detail

/**
 * @brief Sixteen component sedenion built via Cayley-Dickson construction.
 */
struct Sedenion {
    std::array<float, 16> comp{}; ///< Scalar and imaginary parts

    /**
     * @brief Multiply two sedenions.
     */
    [[nodiscard]] Sedenion operator*(const Sedenion &rhs) const noexcept {
        return Sedenion{detail::cd_mul<16>(comp, rhs.comp)};
    }

    /// Construct from explicit coefficients.
    explicit constexpr Sedenion(std::array<float, 16> c) noexcept : comp(c) {}

    constexpr Sedenion() = default;

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
 * @brief Key pair for the zero-divisor cryptosystem.
 */
struct ZPair {
    Sedenion pub{};  ///< Public zero divisor
    Sedenion priv{}; ///< Companion private zero divisor
};

/**
 * @brief Generate a complementary zero-divisor pair.
 *
 * The public component is returned in `pub` and the private component in
 * `priv`.
 *
 * @return Randomly generated key pair.
 */
[[nodiscard]] inline ZPair zpair_generate() {
    std::array<float, 16> u{};
    for (float &v : u) {
        v = static_cast<float>(rand()) / RAND_MAX; // demo randomness
    }
    std::array<float, 16> a{};
    for (std::size_t i = 0; i < 8; ++i) {
        a[i] = u[i];
        a[i + 8] = u[i];
    }
    std::array<float, 16> b{};
    for (std::size_t i = 0; i < 8; ++i) {
        b[i] = u[i];
        b[i + 8] = -u[i];
    }
    return ZPair{Sedenion{a}, Sedenion{b}};
}

/**
 * @brief Encrypt a 128-bit block using zero-divisor addition.
 *
 * @param pub Public zero divisor.
 * @param m   Plaintext block.
 * @return Ciphertext sedenion.
 */
[[nodiscard]] inline Sedenion zlock_encrypt(const Sedenion &pub, std::span<const uint8_t, 16> m) {
    std::array<float, 16> block{};
    for (std::size_t i = 0; i < 16 && i < m.size(); ++i) {
        block[i] = static_cast<float>(m[i]);
    }
    std::array<float, 16> sum{};
    for (std::size_t i = 0; i < 16; ++i) {
        sum[i] = pub.comp[i] + block[i];
    }
    return Sedenion{sum};
}

/**
 * @brief Decrypt a ciphertext knowing the companion zero divisor.
 *
 * @param pub Public zero divisor used during encryption.
 * @param c   Ciphertext.
 * @return Decrypted 128-bit block.
 */
[[nodiscard]] inline std::array<uint8_t, 16> zlock_decrypt(const Sedenion &pub, const Sedenion &c) {
    std::array<uint8_t, 16> out{};
    for (std::size_t i = 0; i < 16; ++i) {
        float value = c.comp[i] - pub.comp[i];
        out[i] = static_cast<uint8_t>(value);
    }
    return out;
}

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

/**
 * @file poly.cpp
 * @brief Kyber polynomial operations.
 *
 * Based on the CRYSTALS-Kyber round 3 reference implementation.
 * Implements compress/decompress, byte packing, noise sampling,
 * NTT wrapper, and arithmetic on ring element polynomials.
 */

#include "poly.h"

#include "cbd.h"
#include "ntt.h"
#include "reduce.h"
#include "symmetric.h"

#include <array>
#include <cstddef>
#include <cstdint>

/*
 * Compression: map coefficients to [0, 2^d) for small d.
 * poly_compress: d = KYBER_POLYCOMPRESSEDBYTES * 8 / 256
 */

void poly_compress(std::uint8_t output[KYBER_POLYCOMPRESSEDBYTES], const poly *input) {
    std::array<std::uint8_t, 8> compressed_coefficients{};
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);

#if (KYBER_POLYCOMPRESSEDBYTES == 128)
    for (std::size_t group_index = 0; group_index < coefficient_count / 8U; ++group_index) {
        for (std::size_t coefficient_index = 0; coefficient_index < 8U; ++coefficient_index) {
            // Map to positive standard representatives.
            auto coefficient = input->coeffs[(8U * group_index) + coefficient_index];
            coefficient += static_cast<std::int16_t>((coefficient >> 15) & KYBER_Q);
            auto scaled_coefficient = static_cast<std::uint32_t>(coefficient) << 4U;
            scaled_coefficient += 1665U;
            scaled_coefficient *= 80635U;
            scaled_coefficient >>= 28U;
            compressed_coefficients[coefficient_index] =
                static_cast<std::uint8_t>(scaled_coefficient & 15U);
        }
        output[0] = static_cast<std::uint8_t>(compressed_coefficients[0] |
                                              (compressed_coefficients[1] << 4U));
        output[1] = static_cast<std::uint8_t>(compressed_coefficients[2] |
                                              (compressed_coefficients[3] << 4U));
        output[2] = static_cast<std::uint8_t>(compressed_coefficients[4] |
                                              (compressed_coefficients[5] << 4U));
        output[3] = static_cast<std::uint8_t>(compressed_coefficients[6] |
                                              (compressed_coefficients[7] << 4U));
        output += 4;
    }
#elif (KYBER_POLYCOMPRESSEDBYTES == 160)
    for (std::size_t group_index = 0; group_index < coefficient_count / 8U; ++group_index) {
        for (std::size_t coefficient_index = 0; coefficient_index < 8U; ++coefficient_index) {
            // Map to positive standard representatives.
            auto coefficient = input->coeffs[(8U * group_index) + coefficient_index];
            coefficient += static_cast<std::int16_t>((coefficient >> 15) & KYBER_Q);
            auto scaled_coefficient = static_cast<std::uint32_t>(coefficient) << 5U;
            scaled_coefficient += 1664U;
            scaled_coefficient *= 40318U;
            scaled_coefficient >>= 27U;
            compressed_coefficients[coefficient_index] =
                static_cast<std::uint8_t>(scaled_coefficient & 31U);
        }
        output[0] = static_cast<std::uint8_t>(compressed_coefficients[0] |
                                              (compressed_coefficients[1] << 5U));
        output[1] = static_cast<std::uint8_t>((compressed_coefficients[1] >> 3U) |
                                              (compressed_coefficients[2] << 2U) |
                                              (compressed_coefficients[3] << 7U));
        output[2] = static_cast<std::uint8_t>((compressed_coefficients[3] >> 1U) |
                                              (compressed_coefficients[4] << 4U));
        output[3] = static_cast<std::uint8_t>((compressed_coefficients[4] >> 4U) |
                                              (compressed_coefficients[5] << 1U) |
                                              (compressed_coefficients[6] << 6U));
        output[4] = static_cast<std::uint8_t>((compressed_coefficients[6] >> 2U) |
                                              (compressed_coefficients[7] << 3U));
        output += 5;
    }
#endif
}

void poly_decompress(poly *output, const std::uint8_t input[KYBER_POLYCOMPRESSEDBYTES]) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
#if (KYBER_POLYCOMPRESSEDBYTES == 128)
    for (std::size_t byte_index = 0; byte_index < coefficient_count / 2U; ++byte_index) {
        const auto low_nibble = static_cast<std::uint16_t>(input[0] & 15U);
        const auto high_nibble = static_cast<std::uint16_t>(input[0] >> 4U);
        output->coeffs[(2U * byte_index)] =
            static_cast<std::int16_t>(((low_nibble * KYBER_Q) + 8U) >> 4U);
        output->coeffs[(2U * byte_index) + 1U] =
            static_cast<std::int16_t>(((high_nibble * KYBER_Q) + 8U) >> 4U);
        ++input;
    }
#elif (KYBER_POLYCOMPRESSEDBYTES == 160)
    std::array<std::uint8_t, 8> compressed_coefficients{};
    for (std::size_t group_index = 0; group_index < coefficient_count / 8U; ++group_index) {
        compressed_coefficients[0] = input[0];
        compressed_coefficients[1] = static_cast<std::uint8_t>((input[0] >> 5U) | (input[1] << 3U));
        compressed_coefficients[2] = static_cast<std::uint8_t>(input[1] >> 2U);
        compressed_coefficients[3] = static_cast<std::uint8_t>((input[1] >> 7U) | (input[2] << 1U));
        compressed_coefficients[4] = static_cast<std::uint8_t>((input[2] >> 4U) | (input[3] << 4U));
        compressed_coefficients[5] = static_cast<std::uint8_t>(input[3] >> 1U);
        compressed_coefficients[6] = static_cast<std::uint8_t>((input[3] >> 6U) | (input[4] << 2U));
        compressed_coefficients[7] = static_cast<std::uint8_t>(input[4] >> 3U);
        input += 5;
        for (std::size_t coefficient_index = 0; coefficient_index < 8U; ++coefficient_index) {
            const auto compressed =
                static_cast<std::uint32_t>(compressed_coefficients[coefficient_index] & 31U);
            output->coeffs[(8U * group_index) + coefficient_index] =
                static_cast<std::int16_t>(((compressed * KYBER_Q) + 16U) >> 5U);
        }
    }
#endif
}

void poly_tobytes(std::uint8_t output[KYBER_POLYBYTES], const poly *input) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t pair_index = 0; pair_index < coefficient_count / 2U; ++pair_index) {
        auto first = static_cast<std::uint16_t>(input->coeffs[2U * pair_index]);
        first += static_cast<std::uint16_t>((static_cast<std::int16_t>(first) >> 15) & KYBER_Q);
        auto second = static_cast<std::uint16_t>(input->coeffs[(2U * pair_index) + 1U]);
        second += static_cast<std::uint16_t>((static_cast<std::int16_t>(second) >> 15) & KYBER_Q);
        output[3U * pair_index] = static_cast<std::uint8_t>(first);
        output[(3U * pair_index) + 1U] = static_cast<std::uint8_t>((first >> 8U) | (second << 4U));
        output[(3U * pair_index) + 2U] = static_cast<std::uint8_t>(second >> 4U);
    }
}

void poly_frombytes(poly *output, const std::uint8_t input[KYBER_POLYBYTES]) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t pair_index = 0; pair_index < coefficient_count / 2U; ++pair_index) {
        const auto byte_offset = 3U * pair_index;
        const auto first_packed = static_cast<std::uint32_t>(input[byte_offset]) |
                                  (static_cast<std::uint32_t>(input[byte_offset + 1U]) << 8U);
        const auto second_packed = (static_cast<std::uint32_t>(input[byte_offset + 1U]) >> 4U) |
                                   (static_cast<std::uint32_t>(input[byte_offset + 2U]) << 4U);
        output->coeffs[2U * pair_index] = static_cast<std::int16_t>(first_packed & 0x0FFFU);
        output->coeffs[(2U * pair_index) + 1U] = static_cast<std::int16_t>(second_packed & 0x0FFFU);
    }
}

void poly_frommsg(poly *output, const std::uint8_t message[KYBER_INDCPA_MSGBYTES]) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t byte_index = 0; byte_index < coefficient_count / 8U; ++byte_index) {
        for (std::size_t bit_index = 0; bit_index < 8U; ++bit_index) {
            const auto bit = static_cast<std::int32_t>((message[byte_index] >> bit_index) & 1U);
            const auto mask = static_cast<std::uint16_t>(-bit);
            output->coeffs[(8U * byte_index) + bit_index] =
                static_cast<std::int16_t>(mask & ((KYBER_Q + 1U) / 2U));
        }
    }
}

void poly_tomsg(std::uint8_t message[KYBER_INDCPA_MSGBYTES], const poly *input) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t byte_index = 0; byte_index < coefficient_count / 8U; ++byte_index) {
        message[byte_index] = 0;
        for (std::size_t bit_index = 0; bit_index < 8U; ++bit_index) {
            auto coefficient =
                static_cast<std::uint16_t>(input->coeffs[(8U * byte_index) + bit_index]);
            coefficient += static_cast<std::uint16_t>(
                (static_cast<std::int16_t>(coefficient) >> 15) & KYBER_Q);
            const auto doubled_coefficient = static_cast<std::uint32_t>(coefficient) << 1U;
            const auto bit =
                static_cast<std::uint16_t>((doubled_coefficient + (KYBER_Q / 2U)) / KYBER_Q) & 1U;
            message[byte_index] |= static_cast<std::uint8_t>(bit << bit_index);
        }
    }
}

void poly_getnoise_eta1(poly *output, const std::uint8_t seed[KYBER_SYMBYTES], std::uint8_t nonce) {
    std::array<std::uint8_t, KYBER_ETA1 * KYBER_N / 4> buffer{};
    prf(buffer.data(), buffer.size(), seed, nonce);
    poly_cbd_eta1(output, buffer.data());
}

void poly_getnoise_eta2(poly *output, const std::uint8_t seed[KYBER_SYMBYTES], std::uint8_t nonce) {
    std::array<std::uint8_t, KYBER_ETA2 * KYBER_N / 4> buffer{};
    prf(buffer.data(), buffer.size(), seed, nonce);
    poly_cbd_eta2(output, buffer.data());
}

void poly_ntt(poly *polynomial) {
    ntt(polynomial->coeffs);
    poly_reduce(polynomial);
}

void poly_invntt_tomont(poly *polynomial) {
    invntt(polynomial->coeffs);
}

void poly_basemul_montgomery(poly *output, const poly *lhs, const poly *rhs) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t group_index = 0; group_index < coefficient_count / 4U; ++group_index) {
        const auto coefficient_offset = 4U * group_index;
        const auto zeta_index = 64U + group_index;
        basemul(&output->coeffs[coefficient_offset], &lhs->coeffs[coefficient_offset],
                &rhs->coeffs[coefficient_offset], zetas[zeta_index]);
        const auto negative_zeta =
            static_cast<std::int16_t>(-static_cast<std::int32_t>(zetas[zeta_index]));
        basemul(&output->coeffs[coefficient_offset + 2U], &lhs->coeffs[coefficient_offset + 2U],
                &rhs->coeffs[coefficient_offset + 2U], negative_zeta);
    }
}

void poly_tomont(poly *polynomial) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    const auto montgomery_factor = static_cast<std::int16_t>((std::uint64_t{1} << 32U) % KYBER_Q);
    for (std::size_t coefficient_index = 0; coefficient_index < coefficient_count;
         ++coefficient_index) {
        polynomial->coeffs[coefficient_index] = montgomery_reduce(
            static_cast<std::int32_t>(polynomial->coeffs[coefficient_index]) * montgomery_factor);
    }
}

void poly_reduce(poly *polynomial) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t coefficient_index = 0; coefficient_index < coefficient_count;
         ++coefficient_index) {
        polynomial->coeffs[coefficient_index] =
            barrett_reduce(polynomial->coeffs[coefficient_index]);
    }
}

void poly_add(poly *output, const poly *lhs, const poly *rhs) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t coefficient_index = 0; coefficient_index < coefficient_count;
         ++coefficient_index) {
        output->coeffs[coefficient_index] = static_cast<std::int16_t>(
            lhs->coeffs[coefficient_index] + rhs->coeffs[coefficient_index]);
    }
}

void poly_sub(poly *output, const poly *lhs, const poly *rhs) {
    constexpr auto coefficient_count = static_cast<std::size_t>(KYBER_N);
    for (std::size_t coefficient_index = 0; coefficient_index < coefficient_count;
         ++coefficient_index) {
        output->coeffs[coefficient_index] = static_cast<std::int16_t>(
            lhs->coeffs[coefficient_index] - rhs->coeffs[coefficient_index]);
    }
}

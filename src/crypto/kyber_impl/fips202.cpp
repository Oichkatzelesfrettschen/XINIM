/**
 * @file fips202.cpp
 * @brief FIPS 202 implementation (SHA-3, SHAKE128, SHAKE256)
 *
 * Implements the Keccak-f[1600] permutation per NIST FIPS 202.
 * All 24 rounds with Theta, Rho, Pi, Chi, Iota step mappings.
 */

#include "fips202.hpp"
#include <cstring>
#include <algorithm>

namespace xinim::crypto::fips202 {

// Keccak round constants (iota step)
static constexpr std::array<std::uint64_t, 24> keccak_rc = {{
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL,
}};

// Rotation offsets (rho step)
static constexpr std::array<int, 25> keccak_rotc = {{
     0,  1, 62, 28, 27,
    36, 44,  6, 55, 20,
     3, 10, 43, 25, 39,
    41, 45, 15, 21,  8,
    18,  2, 61, 56, 14,
}};

// Pi step index mapping: piln[i] = destination index for source index i
// Derived from (x,y) -> (y, 2x+3y mod 5) where linear index = x + 5*y
static constexpr std::array<int, 25> keccak_piln = {{
     0, 10, 20,  5, 15,
    16,  1, 11, 21,  6,
     7, 17,  2, 12, 22,
    23,  8, 18,  3, 13,
    14, 24,  9, 19,  4,
}};

static inline std::uint64_t rotl64(std::uint64_t x, int s) noexcept {
    return (x << s) | (x >> (64 - s));
}

/**
 * @brief Keccak-f[1600] permutation -- 24 rounds.
 */
static void keccakf1600(std::array<std::uint64_t, 25>& st) noexcept {
    for (int round = 0; round < 24; ++round) {
        // Theta
        std::uint64_t bc[5];
        for (int i = 0; i < 5; ++i)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

        for (int i = 0; i < 5; ++i) {
            std::uint64_t t = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }

        // Rho and Pi
        std::uint64_t tmp[25];
        for (int i = 0; i < 25; ++i)
            tmp[keccak_piln[i]] = rotl64(st[i], keccak_rotc[i]);

        // Chi
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; ++i)
                st[j + i] = tmp[j + i] ^ (~tmp[j + (i + 1) % 5] & tmp[j + (i + 2) % 5]);
        }

        // Iota
        st[0] ^= keccak_rc[round];
    }
}

/**
 * @brief Absorb input into Keccak sponge state.
 */
static void keccak_absorb(std::array<std::uint64_t, 25>& s,
                           std::size_t rate,
                           const std::uint8_t* in, std::size_t inlen,
                           std::uint8_t dsb) noexcept {
    std::size_t rate_bytes = rate;

    // Absorb full blocks
    while (inlen >= rate_bytes) {
        for (std::size_t i = 0; i < rate_bytes / 8; ++i) {
            std::uint64_t t = 0;
            std::memcpy(&t, in + i * 8, 8);
            s[i] ^= t;
        }
        keccakf1600(s);
        in += rate_bytes;
        inlen -= rate_bytes;
    }

    // Absorb remaining bytes with padding
    std::array<std::uint8_t, 200> block{};
    std::memcpy(block.data(), in, inlen);
    block[inlen] = dsb;
    block[rate_bytes - 1] |= 0x80;

    for (std::size_t i = 0; i < rate_bytes / 8; ++i) {
        std::uint64_t t = 0;
        std::memcpy(&t, &block[i * 8], 8);
        s[i] ^= t;
    }
    keccakf1600(s);
}

/**
 * @brief Squeeze output from Keccak sponge state.
 */
static void keccak_squeeze(std::uint8_t* out, std::size_t outlen,
                            std::array<std::uint64_t, 25>& s,
                            std::size_t rate) noexcept {
    std::size_t rate_bytes = rate;

    while (outlen > 0) {
        std::size_t block = (outlen < rate_bytes) ? outlen : rate_bytes;
        for (std::size_t i = 0; i < block; ++i) {
            out[i] = static_cast<std::uint8_t>(s[i / 8] >> (8 * (i % 8)));
        }
        out += block;
        outlen -= block;
        if (outlen > 0)
            keccakf1600(s);
    }
}

/**
 * @brief Absorb-only for incremental squeeze interface.
 */
static void keccak_absorb_once(keccak_state& state, std::size_t rate,
                                const std::uint8_t* in, std::size_t inlen,
                                std::uint8_t dsb) noexcept {
    state.s = {};
    state.pos = 0;
    keccak_absorb(state.s, rate, in, inlen, dsb);
}

/**
 * @brief Squeeze full blocks from state (incremental).
 */
static void keccak_squeezeblocks(std::uint8_t* out, std::size_t nblocks,
                                  std::array<std::uint64_t, 25>& s,
                                  std::size_t rate) noexcept {
    while (nblocks > 0) {
        for (std::size_t i = 0; i < rate / 8; ++i) {
            std::memcpy(out + i * 8, &s[i], 8);
        }
        keccakf1600(s);
        out += rate;
        --nblocks;
    }
}

// Error category for FIPS 202
class fips202_error_category : public std::error_category {
public:
    const char* name() const noexcept override {
        return "fips202";
    }

    std::string message(int ev) const override {
        switch (static_cast<fips202_error>(ev)) {
            case fips202_error::invalid_input_length:
                return "Invalid input length";
            case fips202_error::invalid_output_length:
                return "Invalid output length";
            case fips202_error::state_corruption:
                return "State corruption detected";
            default:
                return "Unknown FIPS 202 error";
        }
    }
};

const std::error_category& fips202_category() noexcept {
    static const fips202_error_category category;
    return category;
}

std::error_code make_error_code(fips202_error e) noexcept {
    return {static_cast<int>(e), fips202_category()};
}

// ============================================================================
// Public API implementations
// ============================================================================

fips202_result shake128(std::span<std::uint8_t> out,
                        std::span<const std::uint8_t> in) noexcept {
    if (out.empty()) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    std::array<std::uint64_t, 25> s{};
    keccak_absorb(s, SHAKE128_RATE, in.data(), in.size(), 0x1F);
    keccak_squeeze(out.data(), out.size(), s, SHAKE128_RATE);
    return {};
}

fips202_result shake256(std::span<std::uint8_t> out,
                        std::span<const std::uint8_t> in) noexcept {
    if (out.empty()) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    std::array<std::uint64_t, 25> s{};
    keccak_absorb(s, SHAKE256_RATE, in.data(), in.size(), 0x1F);
    keccak_squeeze(out.data(), out.size(), s, SHAKE256_RATE);
    return {};
}

fips202_result shake128_absorb(keccak_state& state,
                               std::span<const std::uint8_t> in) noexcept {
    keccak_absorb_once(state, SHAKE128_RATE, in.data(), in.size(), 0x1F);
    return {};
}

fips202_result shake256_absorb(keccak_state& state,
                               std::span<const std::uint8_t> in) noexcept {
    keccak_absorb_once(state, SHAKE256_RATE, in.data(), in.size(), 0x1F);
    return {};
}

fips202_result shake128_squeezeblocks(std::span<std::uint8_t> out,
                                      keccak_state& state) noexcept {
    if (out.size() % SHAKE128_RATE != 0) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    std::size_t nblocks = out.size() / SHAKE128_RATE;
    keccak_squeezeblocks(out.data(), nblocks, state.s, SHAKE128_RATE);
    return {};
}

fips202_result shake256_squeezeblocks(std::span<std::uint8_t> out,
                                      keccak_state& state) noexcept {
    if (out.size() % SHAKE256_RATE != 0) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    std::size_t nblocks = out.size() / SHAKE256_RATE;
    keccak_squeezeblocks(out.data(), nblocks, state.s, SHAKE256_RATE);
    return {};
}

fips202_result shake256_squeeze(std::span<std::uint8_t> out,
                                keccak_state& state) noexcept {
    if (out.empty()) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    keccak_squeeze(out.data(), out.size(), state.s, SHAKE256_RATE);
    return {};
}

fips202_result sha3_256(std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE> out,
                        std::span<const std::uint8_t> in) noexcept {
    std::array<std::uint64_t, 25> s{};
    // SHA3-256 uses rate = 1088 bits = 136 bytes, domain separator = 0x06
    keccak_absorb(s, 136, in.data(), in.size(), 0x06);
    keccak_squeeze(out.data(), SHA3_256_OUTPUT_SIZE, s, 136);
    return {};
}

fips202_result sha3_512(std::span<std::uint8_t, SHA3_512_OUTPUT_SIZE> out,
                        std::span<const std::uint8_t> in) noexcept {
    std::array<std::uint64_t, 25> s{};
    // SHA3-512 uses rate = 576 bits = 72 bytes, domain separator = 0x06
    keccak_absorb(s, 72, in.data(), in.size(), 0x06);
    keccak_squeeze(out.data(), SHA3_512_OUTPUT_SIZE, s, 72);
    return {};
}

// Convenience functions

std::expected<std::vector<std::uint8_t>, error_code>
shake128(std::span<const std::uint8_t> in, std::size_t output_length) {
    if (output_length == 0) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    std::vector<std::uint8_t> result(output_length);
    auto span_result = std::span<std::uint8_t>{result};

    if (auto err = shake128(span_result, in)) {
        return result;
    } else {
        return std::unexpected(err.error());
    }
}

std::expected<std::vector<std::uint8_t>, error_code>
shake256(std::span<const std::uint8_t> in, std::size_t output_length) {
    if (output_length == 0) {
        return std::unexpected(make_error_code(fips202_error::invalid_output_length));
    }

    std::vector<std::uint8_t> result(output_length);
    auto span_result = std::span<std::uint8_t>{result};

    if (auto err = shake256(span_result, in)) {
        return result;
    } else {
        return std::unexpected(err.error());
    }
}

std::expected<std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE>, error_code>
sha3_256(std::span<const std::uint8_t> in) {
    std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE> result{};
    auto span_result = std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE>{result};

    if (auto err = sha3_256(span_result, in)) {
        return result;
    } else {
        return std::unexpected(err.error());
    }
}

std::expected<std::array<std::uint8_t, SHA3_512_OUTPUT_SIZE>, error_code>
sha3_512(std::span<const std::uint8_t> in) {
    std::array<std::uint8_t, SHA3_512_OUTPUT_SIZE> result{};
    auto span_result = std::span<std::uint8_t, SHA3_512_OUTPUT_SIZE>{result};

    if (auto err = sha3_512(span_result, in)) {
        return result;
    } else {
        return std::unexpected(err.error());
    }
}

} // namespace xinim::crypto::fips202

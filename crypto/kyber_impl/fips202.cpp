// FIPS202 (SHA3/SHAKE) Implementation - C++23 with HAL
// Based on public domain implementations from SUPERCOP and TweetFips202

#include "fips202.hpp"
#include <xinim/hal/arch.hpp>
#include <xinim/hal/simd.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <ranges>

namespace xinim::crypto::kyber {

namespace {

constexpr size_t NROUNDS = 24;

// Optimized rotation using std::rotl (C++20)
template<typename T>
constexpr T ROL(T a, int offset) noexcept {
    return std::rotl(a, offset);
}

// Load/store with endianness handling using C++23 features
inline uint64_t load64_le(const uint8_t* x) noexcept {
    uint64_t value;
    std::memcpy(&value, x, 8);
    if constexpr (std::endian::native == std::endian::big) {
        return std::byteswap(value);
    }
    return value;
}

inline void store64_le(uint8_t* x, uint64_t u) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        u = std::byteswap(u);
    }
    std::memcpy(x, &u, 8);
}

// Keccak round constants
constexpr std::array<uint64_t, NROUNDS> KeccakF_RoundConstants = {
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
    0x0000000080000001ULL, 0x8000000080008008ULL
};

// Modern C++23 KeccakF1600 state permutation with architecture optimization
class KeccakF1600 {
    std::array<uint64_t, 25> state{};
    
public:
    void permute() noexcept {
        // Architecture-specific optimizations
        if constexpr (xinim::hal::is_x86_64) {
            permute_x86_64();
        } else if constexpr (xinim::hal::is_arm64) {
            permute_arm64();
        } else {
            permute_generic();
        }
    }
    
    void absorb(std::span<const uint8_t> data, size_t rate) noexcept {
        size_t offset = 0;
        while (offset + rate <= data.size()) {
            xor_block(data.subspan(offset, rate), rate);
            permute();
            offset += rate;
        }
    }
    
    void squeeze(std::span<uint8_t> out, size_t rate) noexcept {
        size_t offset = 0;
        while (offset + rate <= out.size()) {
            extract_block(out.subspan(offset, rate), rate);
            permute();
            offset += rate;
        }
        if (offset < out.size()) {
            extract_block(out.subspan(offset), out.size() - offset);
        }
    }
    
    void xor_block(std::span<const uint8_t> data, size_t len) noexcept {
        size_t words = len / 8;
        for (size_t i = 0; i < words; ++i) {
            state[i] ^= load64_le(&data[i * 8]);
        }
        if (len % 8) {
            uint8_t temp[8] = {};
            std::copy_n(&data[words * 8], len % 8, temp);
            state[words] ^= load64_le(temp);
        }
    }
    
    void extract_block(std::span<uint8_t> out, size_t len) noexcept {
        size_t words = len / 8;
        for (size_t i = 0; i < words; ++i) {
            store64_le(&out[i * 8], state[i]);
        }
        if (len % 8) {
            uint8_t temp[8];
            store64_le(temp, state[words]);
            std::copy_n(temp, len % 8, &out[words * 8]);
        }
    }
    
private:
    void permute_generic() noexcept {
        for (const auto& rc : KeccakF_RoundConstants) {
            keccak_round(rc);
        }
    }
    
    void permute_x86_64() noexcept {
        // X86-64 specific optimizations with prefetching
        xinim::hal::prefetch<xinim::hal::prefetch_hint::read_high>(state.data());
        permute_generic(); // For now, fallback to generic
    }
    
    void permute_arm64() noexcept {
        // ARM64 specific optimizations
        xinim::hal::prefetch<xinim::hal::prefetch_hint::read_low>(state.data());
        permute_generic(); // For now, fallback to generic
    }
    
    void keccak_round(uint64_t rc) noexcept {
        // Theta step
        std::array<uint64_t, 5> C;
        for (size_t x = 0; x < 5; ++x) {
            C[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }
        
        std::array<uint64_t, 5> D;
        for (size_t x = 0; x < 5; ++x) {
            D[x] = C[(x + 4) % 5] ^ ROL(C[(x + 1) % 5], 1);
        }
        
        for (size_t x = 0; x < 5; ++x) {
            for (size_t y = 0; y < 5; ++y) {
                state[y * 5 + x] ^= D[x];
            }
        }
        
        // Rho and Pi steps
        uint64_t current = state[1];
        for (const auto& [x, y, r] : rho_pi_offsets) {
            size_t index = y * 5 + x;
            uint64_t temp = state[index];
            state[index] = ROL(current, r);
            current = temp;
        }
        
        // Chi step
        for (size_t y = 0; y < 5; ++y) {
            std::array<uint64_t, 5> T;
            for (size_t x = 0; x < 5; ++x) {
                T[x] = state[y * 5 + x];
            }
            for (size_t x = 0; x < 5; ++x) {
                state[y * 5 + x] = T[x] ^ ((~T[(x + 1) % 5]) & T[(x + 2) % 5]);
            }
        }
        
        // Iota step
        state[0] ^= rc;
    }
    
    // Precomputed rho-pi offsets
    static constexpr std::array<std::tuple<int, int, int>, 24> rho_pi_offsets = {{
        {0, 1, 44}, {1, 2, 43}, {2, 3, 21}, {3, 4, 14},
        {4, 0, 28}, {0, 2, 20}, {2, 4, 3}, {4, 1, 45},
        {1, 3, 61}, {3, 0, 39}, {0, 4, 18}, {4, 3, 62},
        {3, 2, 56}, {2, 1, 55}, {1, 0, 36}, {0, 3, 27},
        {3, 1, 41}, {1, 4, 2}, {4, 2, 23}, {2, 0, 8},
        {0, 0, 25}, {1, 1, 10}, {2, 2, 15}, {3, 3, 24}
    }};
};

} // anonymous namespace

// Public API implementation
void shake128_absorb_once(shake128ctx* state, 
                          const uint8_t* input, 
                          size_t inlen) noexcept {
    auto* ctx = reinterpret_cast<KeccakF1600*>(state);
    ctx->xor_block({input, inlen}, inlen);
    
    // Padding
    uint8_t pad[2] = {0x1F, 0x80};
    ctx->xor_block({pad, 1}, 1);
    ctx->xor_block({pad + 1, 1}, SHAKE128_RATE - 1);
    ctx->permute();
}

void shake128_squeeze(uint8_t* output, 
                     size_t outlen, 
                     shake128ctx* state) noexcept {
    auto* ctx = reinterpret_cast<KeccakF1600*>(state);
    ctx->squeeze({output, outlen}, SHAKE128_RATE);
}

void shake256_absorb_once(shake256ctx* state,
                          const uint8_t* input,
                          size_t inlen) noexcept {
    auto* ctx = reinterpret_cast<KeccakF1600*>(state);
    ctx->xor_block({input, inlen}, inlen);
    
    // Padding
    uint8_t pad[2] = {0x1F, 0x80};
    ctx->xor_block({pad, 1}, 1);
    ctx->xor_block({pad + 1, 1}, SHAKE256_RATE - 1);
    ctx->permute();
}

void shake256_squeeze(uint8_t* output,
                     size_t outlen, 
                     shake256ctx* state) noexcept {
    auto* ctx = reinterpret_cast<KeccakF1600*>(state);
    ctx->squeeze({output, outlen}, SHAKE256_RATE);
}

void shake128(uint8_t* output, size_t outlen,
              const uint8_t* input, size_t inlen) noexcept {
    shake128ctx ctx;
    shake128_absorb_once(&ctx, input, inlen);
    shake128_squeeze(output, outlen, &ctx);
}

void shake256(uint8_t* output, size_t outlen,
              const uint8_t* input, size_t inlen) noexcept {
    shake256ctx ctx;
    shake256_absorb_once(&ctx, input, inlen);
    shake256_squeeze(output, outlen, &ctx);
}

void sha3_256(uint8_t* output, const uint8_t* input, size_t inlen) noexcept {
    KeccakF1600 state;
    constexpr size_t rate = 136; // SHA3-256 rate
    
    // Absorb
    state.absorb({input, inlen}, rate);
    
    // Padding
    uint8_t pad[2] = {0x06, 0x80};
    state.xor_block({pad, 1}, 1);
    state.xor_block({pad + 1, 1}, rate - 1);
    state.permute();
    
    // Squeeze
    state.extract_block({output, 32}, 32);
}

void sha3_512(uint8_t* output, const uint8_t* input, size_t inlen) noexcept {
    KeccakF1600 state;
    constexpr size_t rate = 72; // SHA3-512 rate
    
    // Absorb
    state.absorb({input, inlen}, rate);
    
    // Padding
    uint8_t pad[2] = {0x06, 0x80};
    state.xor_block({pad, 1}, 1);
    state.xor_block({pad + 1, 1}, rate - 1);
    state.permute();
    
    // Squeeze
    state.extract_block({output, 64}, 64);
}

} // namespace xinim::crypto::kyber
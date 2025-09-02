// FIPS202 (SHA3/SHAKE) Header - C++23
#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::crypto::kyber {

// Rate constants for SHAKE
constexpr size_t SHAKE128_RATE = 168;
constexpr size_t SHAKE256_RATE = 136;

// Opaque context structures (actual implementation hidden)
struct alignas(64) shake128ctx {
    uint64_t state[25];
};

struct alignas(64) shake256ctx {
    uint64_t state[25];
};

// SHAKE128 functions
void shake128_absorb_once(shake128ctx* state, 
                          const uint8_t* input, 
                          size_t inlen) noexcept;

void shake128_squeeze(uint8_t* output, 
                     size_t outlen, 
                     shake128ctx* state) noexcept;

void shake128(uint8_t* output, size_t outlen,
              const uint8_t* input, size_t inlen) noexcept;

// SHAKE256 functions
void shake256_absorb_once(shake256ctx* state,
                          const uint8_t* input,
                          size_t inlen) noexcept;

void shake256_squeeze(uint8_t* output,
                     size_t outlen,
                     shake256ctx* state) noexcept;

void shake256(uint8_t* output, size_t outlen,
              const uint8_t* input, size_t inlen) noexcept;

// SHA3 functions
void sha3_256(uint8_t* output, const uint8_t* input, size_t inlen) noexcept;
void sha3_512(uint8_t* output, const uint8_t* input, size_t inlen) noexcept;

} // namespace xinim::crypto::kyber

// C-compatible wrapper for legacy code
extern "C" {
    using shake128ctx = xinim::crypto::kyber::shake128ctx;
    using shake256ctx = xinim::crypto::kyber::shake256ctx;
    
    #define SHAKE128_RATE xinim::crypto::kyber::SHAKE128_RATE
    #define SHAKE256_RATE xinim::crypto::kyber::SHAKE256_RATE
    
    inline void shake128_absorb_once(shake128ctx* s, const uint8_t* in, size_t inlen) {
        xinim::crypto::kyber::shake128_absorb_once(s, in, inlen);
    }
    
    inline void shake128_squeeze(uint8_t* out, size_t outlen, shake128ctx* s) {
        xinim::crypto::kyber::shake128_squeeze(out, outlen, s);
    }
    
    inline void shake256_absorb_once(shake256ctx* s, const uint8_t* in, size_t inlen) {
        xinim::crypto::kyber::shake256_absorb_once(s, in, inlen);
    }
    
    inline void shake256_squeeze(uint8_t* out, size_t outlen, shake256ctx* s) {
        xinim::crypto::kyber::shake256_squeeze(out, outlen, s);
    }
    
    inline void shake128(uint8_t* out, size_t outlen, const uint8_t* in, size_t inlen) {
        xinim::crypto::kyber::shake128(out, outlen, in, inlen);
    }
    
    inline void shake256(uint8_t* out, size_t outlen, const uint8_t* in, size_t inlen) {
        xinim::crypto::kyber::shake256(out, outlen, in, inlen);
    }
    
    inline void sha3_256(uint8_t* out, const uint8_t* in, size_t inlen) {
        xinim::crypto::kyber::sha3_256(out, in, inlen);
    }
    
    inline void sha3_512(uint8_t* out, const uint8_t* in, size_t inlen) {
        xinim::crypto::kyber::sha3_512(out, in, inlen);
    }
}
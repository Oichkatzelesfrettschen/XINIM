/**
 * @file symmetric.cpp
 * @brief C-style symmetric primitive wrappers for Kyber.
 *
 * Bridges the KYBER_NAMESPACE C function declarations in symmetric.h to the
 * xinim::crypto::fips202 namespace implementation in fips202.cpp.
 *
 * Three functions are needed by the rest of the Kyber implementation:
 *   kyber_shake128_absorb  -- absorb (seed || row || col) into SHAKE128 state
 *   kyber_shake256_prf     -- SHAKE256(key || nonce) -> outlen bytes
 *   kyber_shake256_rkprf   -- SHAKE256(key || ciphertext) -> KYBER_SSBYTES
 *
 * The macro shims in symmetric.h also reference:
 *   sha3_256(out, in, inlen)
 *   sha3_512(out, in, inlen)
 *   shake128_squeezeblocks(out, nblocks, state)
 * which are provided here as plain C-style wrappers.
 */

#include "symmetric.h"
#include "fips202.hpp"
#include "params.h"
#include <stdint.h>
#include <stddef.h>
#include <cstring>

using namespace xinim::crypto::fips202;

// ---------------------------------------------------------------------------
// Plain C-style sha3/shake wrappers (called by macros in symmetric.h)
// ---------------------------------------------------------------------------

/*
 * sha3_256(out, in, inlen) -- produces 32 bytes.
 * Called via: hash_h(OUT, IN, INBYTES) macro.
 */
void sha3_256(uint8_t out[32], const uint8_t *in, size_t inlen) {
    std::array<uint8_t, SHA3_256_OUTPUT_SIZE> buf{};
    (void)sha3_256(std::span<uint8_t, SHA3_256_OUTPUT_SIZE>{buf},
                   std::span<const uint8_t>{in, inlen});
    std::memcpy(out, buf.data(), 32);
}

/*
 * sha3_512(out, in, inlen) -- produces 64 bytes.
 * Called via: hash_g(OUT, IN, INBYTES) macro.
 */
void sha3_512(uint8_t out[64], const uint8_t *in, size_t inlen) {
    std::array<uint8_t, SHA3_512_OUTPUT_SIZE> buf{};
    (void)sha3_512(std::span<uint8_t, SHA3_512_OUTPUT_SIZE>{buf},
                   std::span<const uint8_t>{in, inlen});
    std::memcpy(out, buf.data(), 64);
}

/*
 * shake128_squeezeblocks(out, nblocks, state)
 * Called via: xof_squeezeblocks(OUT, OUTBLOCKS, STATE) macro.
 */
void shake128_squeezeblocks(uint8_t *out, size_t nblocks,
                            keccak_state *state) {
    (void)shake128_squeezeblocks(
        std::span<uint8_t>{out, nblocks * SHAKE128_RATE},
        *state);
}

// ---------------------------------------------------------------------------
// Kyber-specific symmetric primitives
// ---------------------------------------------------------------------------

/*
 * kyber_shake128_absorb: seed (32 bytes) || x || y -> SHAKE128 state.
 * Used by gen_matrix to expand the matrix A from a public seed.
 */
void kyber_shake128_absorb(keccak_state *s,
                           const uint8_t seed[KYBER_SYMBYTES],
                           uint8_t x,
                           uint8_t y) {
    uint8_t extseed[KYBER_SYMBYTES + 2];
    std::memcpy(extseed, seed, KYBER_SYMBYTES);
    extseed[KYBER_SYMBYTES]     = x;
    extseed[KYBER_SYMBYTES + 1] = y;
    (void)shake128_absorb(*s, std::span<const uint8_t>{extseed, sizeof(extseed)});
}

/*
 * kyber_shake256_prf: SHAKE256(key || nonce) -> outlen bytes.
 * Used by poly_getnoise_eta1/eta2 via the prf() macro.
 */
void kyber_shake256_prf(uint8_t *out, size_t outlen,
                        const uint8_t key[KYBER_SYMBYTES],
                        uint8_t nonce) {
    uint8_t extkey[KYBER_SYMBYTES + 1];
    std::memcpy(extkey, key, KYBER_SYMBYTES);
    extkey[KYBER_SYMBYTES] = nonce;

    keccak_state state{};
    (void)shake256_absorb(state, std::span<const uint8_t>{extkey, sizeof(extkey)});
    (void)shake256_squeeze(std::span<uint8_t>{out, outlen}, state);
}

/*
 * kyber_shake256_rkprf: SHAKE256(key || ciphertext) -> KYBER_SSBYTES bytes.
 * Used in crypto_kem_dec implicit rejection path via rkprf() macro.
 */
void kyber_shake256_rkprf(uint8_t out[KYBER_SSBYTES],
                           const uint8_t key[KYBER_SYMBYTES],
                           const uint8_t input[KYBER_CIPHERTEXTBYTES]) {
    constexpr size_t inlen = KYBER_SYMBYTES + KYBER_CIPHERTEXTBYTES;
    uint8_t buf[inlen];
    std::memcpy(buf, key, KYBER_SYMBYTES);
    std::memcpy(buf + KYBER_SYMBYTES, input, KYBER_CIPHERTEXTBYTES);

    keccak_state state{};
    (void)shake256_absorb(state, std::span<const uint8_t>{buf, inlen});
    (void)shake256_squeeze(std::span<uint8_t>{out, KYBER_SSBYTES}, state);
}

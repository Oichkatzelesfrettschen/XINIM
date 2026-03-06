/**
 * @file kem.cpp
 * @brief Kyber IND-CCA2-secure Key Encapsulation Mechanism (ML-KEM).
 *
 * Implements the Fujisaki-Okamoto transform over Kyber IND-CPA to obtain
 * IND-CCA2 security.  Based on CRYSTALS-Kyber round 3 reference (FIPS 203).
 *
 * crypto_kem_keypair_derand: deterministic keypair from 2*32 coin bytes
 * crypto_kem_keypair:        generate fresh keypair using OS RNG
 * crypto_kem_enc_derand:     deterministic encapsulation from 32 coin bytes
 * crypto_kem_enc:            encapsulate using OS RNG
 * crypto_kem_dec:            decapsulate; implicit rejection on failure
 */

#include "kem.h"
#include "indcpa.h"
#include "verify.h"
#include "symmetric.h"
#include "randombytes.hpp"
#include "params.h"
#include <stdint.h>
#include <cstring>

using xinim::crypto::random::randombytes;

// ---------------------------------------------------------------------------
// Deterministic keypair generation
// ---------------------------------------------------------------------------

int crypto_kem_keypair_derand(uint8_t *pk, uint8_t *sk,
                               const uint8_t *coins) {
    indcpa_keypair_derand(pk, sk, coins);

    // sk layout:
    //   [0  .. KYBER_INDCPA_SECRETKEYBYTES)  IND-CPA secret key
    //   [KYBER_INDCPA_SECRETKEYBYTES .. +KYBER_PUBLICKEYBYTES) public key copy
    //   [.. + KYBER_SYMBYTES) H(pk)
    //   [.. + KYBER_SYMBYTES) z (implicit-rejection randomness)

    uint8_t *sk_base = sk;
    std::memcpy(sk_base + KYBER_INDCPA_SECRETKEYBYTES, pk, KYBER_PUBLICKEYBYTES);

    hash_h(sk_base + KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES,
           pk, KYBER_PUBLICKEYBYTES);

    std::memcpy(sk_base + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES,
                coins + KYBER_SYMBYTES, KYBER_SYMBYTES);
    return 0;
}

// ---------------------------------------------------------------------------
// Randomized keypair generation
// ---------------------------------------------------------------------------

int crypto_kem_keypair(uint8_t *pk, uint8_t *sk) {
    uint8_t coins[2 * KYBER_SYMBYTES];
    auto result = randombytes(std::span<uint8_t>{coins, sizeof(coins)});
    if (!result) {
        return -1;
    }
    return crypto_kem_keypair_derand(pk, sk, coins);
}

// ---------------------------------------------------------------------------
// Deterministic encapsulation
// ---------------------------------------------------------------------------

int crypto_kem_enc_derand(uint8_t *ct, uint8_t *ss,
                           const uint8_t *pk,
                           const uint8_t *coins) {
    // buf = coins || H(pk), then hash_g for the final shared secret seed
    uint8_t buf[2 * KYBER_SYMBYTES];
    uint8_t kr[2 * KYBER_SYMBYTES];

    std::memcpy(buf, coins, KYBER_SYMBYTES);
    hash_h(buf + KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
    hash_g(kr, buf, 2 * KYBER_SYMBYTES);

    // kr[0..KYBER_SYMBYTES) = session key (K_bar)
    // kr[KYBER_SYMBYTES..)  = randomness r for IND-CPA enc

    indcpa_enc(ct, coins, pk, kr + KYBER_SYMBYTES);

    std::memcpy(ss, kr, KYBER_SSBYTES);
    return 0;
}

// ---------------------------------------------------------------------------
// Randomized encapsulation
// ---------------------------------------------------------------------------

int crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk) {
    uint8_t coins[KYBER_SYMBYTES];
    auto result = randombytes(std::span<uint8_t>{coins, sizeof(coins)});
    if (!result) {
        return -1;
    }
    return crypto_kem_enc_derand(ct, ss, pk, coins);
}

// ---------------------------------------------------------------------------
// Decapsulation
// ---------------------------------------------------------------------------

int crypto_kem_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk) {
    // sk layout:
    //   [0 .. KYBER_INDCPA_SECRETKEYBYTES) IND-CPA sk
    //   [KYBER_INDCPA_SECRETKEYBYTES .. +KYBER_PUBLICKEYBYTES) pk copy
    //   [.. + KYBER_SYMBYTES) H(pk)
    //   [.. + KYBER_SYMBYTES) z

    const uint8_t *pk   = sk + KYBER_INDCPA_SECRETKEYBYTES;
    const uint8_t *hpk  = sk + KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES;
    const uint8_t *z    = sk + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES;

    // Re-decrypt to get the candidate message
    uint8_t m[KYBER_INDCPA_MSGBYTES];
    indcpa_dec(m, ct, sk);

    // Re-derive shared-secret candidate: buf = m || H(pk)
    uint8_t buf[2 * KYBER_SYMBYTES];
    std::memcpy(buf, m, KYBER_INDCPA_MSGBYTES);
    std::memcpy(buf + KYBER_SYMBYTES, hpk, KYBER_SYMBYTES);

    uint8_t kr[2 * KYBER_SYMBYTES];
    hash_g(kr, buf, 2 * KYBER_SYMBYTES);

    // Re-encrypt under pk using the re-derived randomness
    uint8_t ct2[KYBER_CIPHERTEXTBYTES];
    indcpa_enc(ct2, m, pk, kr + KYBER_SYMBYTES);

    // Constant-time comparison
    int fail = verify(ct, ct2, KYBER_CIPHERTEXTBYTES);

    // Implicit rejection: ss = SHAKE256(z || ct) on failure
    uint8_t ss_cand[KYBER_SSBYTES];
    rkprf(ss_cand, z, ct);

    // Select correct shared secret in constant time
    cmov(ss_cand, kr, KYBER_SSBYTES, static_cast<uint8_t>(1 - fail));
    std::memcpy(ss, ss_cand, KYBER_SSBYTES);
    return 0;
}

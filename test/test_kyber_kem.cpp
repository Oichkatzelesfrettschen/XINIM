/**
 * @file test_kyber_kem.cpp
 * @brief Host-side KEM correctness tests for Kyber768 (default).
 *
 * Validates:
 *   1. Keypair generation produces non-zero output.
 *   2. Encapsulation / decapsulation round-trip: ss_enc == ss_dec.
 *   3. Ciphertext corruption causes implicit rejection: ss != ss_dec.
 *   4. Deterministic API: same coins -> same (pk, sk, ct, ss).
 *   5. Different coins -> different shared secrets.
 */

// params.h expects KYBER_K to be defined before inclusion; default is 3.
#include "../src/crypto/kyber_impl/params.h"
#include "../src/crypto/kyber_impl/kem.h"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Minimal test harness (no external deps)
// ---------------------------------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond) \
    do { if (!(cond)) { \
        printf("FAIL: %s:%d: " #cond "\n", __FILE__, __LINE__); \
        return 1; \
    } } while (0)

static void run_test(const char *name, int (*fn)()) {
    if (fn() == 0) {
        printf("PASS: %s\n", name);
        ++g_passed;
    } else {
        printf("FAIL: %s\n", name);
        ++g_failed;
    }
}

// ---------------------------------------------------------------------------
// Deterministic seed helpers
// ---------------------------------------------------------------------------

static void fill_seed(uint8_t *buf, size_t len, uint8_t val) {
    for (size_t i = 0; i < len; ++i) buf[i] = val;
}

static bool bytes_nonzero(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) if (buf[i]) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Test 1: keypair output is non-zero
// ---------------------------------------------------------------------------
static int test_keypair_nonzero() {
    uint8_t pk[KYBER_PUBLICKEYBYTES]  = {};
    uint8_t sk[KYBER_SECRETKEYBYTES]  = {};
    uint8_t coins[2 * KYBER_SYMBYTES] = {};
    fill_seed(coins, sizeof(coins), 0x01);

    CHECK(crypto_kem_keypair_derand(pk, sk, coins) == 0);
    CHECK(bytes_nonzero(pk, sizeof(pk)));
    CHECK(bytes_nonzero(sk, sizeof(sk)));
    return 0;
}

// ---------------------------------------------------------------------------
// Test 2: enc/dec round-trip
// ---------------------------------------------------------------------------
static int test_encap_decap_roundtrip() {
    uint8_t pk[KYBER_PUBLICKEYBYTES]  = {};
    uint8_t sk[KYBER_SECRETKEYBYTES]  = {};
    uint8_t ct[KYBER_CIPHERTEXTBYTES] = {};
    uint8_t ss_enc[KYBER_SSBYTES]     = {};
    uint8_t ss_dec[KYBER_SSBYTES]     = {};
    uint8_t kp_coins[2 * KYBER_SYMBYTES] = {};
    uint8_t enc_coins[KYBER_SYMBYTES]    = {};
    fill_seed(kp_coins,  sizeof(kp_coins),  0xAA);
    fill_seed(enc_coins, sizeof(enc_coins), 0xBB);

    CHECK(crypto_kem_keypair_derand(pk, sk, kp_coins)             == 0);
    CHECK(crypto_kem_enc_derand(ct, ss_enc, pk, enc_coins)        == 0);
    CHECK(crypto_kem_dec(ss_dec, ct, sk)                          == 0);

    CHECK(__builtin_memcmp(ss_enc, ss_dec, KYBER_SSBYTES) == 0);
    return 0;
}

// ---------------------------------------------------------------------------
// Test 3: ciphertext corruption triggers implicit rejection
// ---------------------------------------------------------------------------
static int test_ciphertext_corruption() {
    uint8_t pk[KYBER_PUBLICKEYBYTES]  = {};
    uint8_t sk[KYBER_SECRETKEYBYTES]  = {};
    uint8_t ct[KYBER_CIPHERTEXTBYTES] = {};
    uint8_t ss_enc[KYBER_SSBYTES]     = {};
    uint8_t ss_dec[KYBER_SSBYTES]     = {};
    uint8_t kp_coins[2 * KYBER_SYMBYTES] = {};
    uint8_t enc_coins[KYBER_SYMBYTES]    = {};
    fill_seed(kp_coins,  sizeof(kp_coins),  0x11);
    fill_seed(enc_coins, sizeof(enc_coins), 0x22);

    CHECK(crypto_kem_keypair_derand(pk, sk, kp_coins)      == 0);
    CHECK(crypto_kem_enc_derand(ct, ss_enc, pk, enc_coins) == 0);

    // Flip one byte in the ciphertext
    ct[0] ^= 0xFF;
    CHECK(crypto_kem_dec(ss_dec, ct, sk) == 0);

    // Shared secrets must differ after corruption
    CHECK(__builtin_memcmp(ss_enc, ss_dec, KYBER_SSBYTES) != 0);
    return 0;
}

// ---------------------------------------------------------------------------
// Test 4: determinism -- same coins -> same output
// ---------------------------------------------------------------------------
static int test_determinism() {
    uint8_t pk1[KYBER_PUBLICKEYBYTES], pk2[KYBER_PUBLICKEYBYTES];
    uint8_t sk1[KYBER_SECRETKEYBYTES], sk2[KYBER_SECRETKEYBYTES];
    uint8_t ct1[KYBER_CIPHERTEXTBYTES], ct2[KYBER_CIPHERTEXTBYTES];
    uint8_t ss1[KYBER_SSBYTES], ss2[KYBER_SSBYTES];
    uint8_t kp_coins[2 * KYBER_SYMBYTES] = {};
    uint8_t enc_coins[KYBER_SYMBYTES]    = {};
    fill_seed(kp_coins,  sizeof(kp_coins),  0xCC);
    fill_seed(enc_coins, sizeof(enc_coins), 0xDD);

    CHECK(crypto_kem_keypair_derand(pk1, sk1, kp_coins)       == 0);
    CHECK(crypto_kem_keypair_derand(pk2, sk2, kp_coins)       == 0);
    CHECK(__builtin_memcmp(pk1, pk2, KYBER_PUBLICKEYBYTES) == 0);
    CHECK(__builtin_memcmp(sk1, sk2, KYBER_SECRETKEYBYTES) == 0);

    CHECK(crypto_kem_enc_derand(ct1, ss1, pk1, enc_coins)     == 0);
    CHECK(crypto_kem_enc_derand(ct2, ss2, pk2, enc_coins)     == 0);
    CHECK(__builtin_memcmp(ct1, ct2, KYBER_CIPHERTEXTBYTES)   == 0);
    CHECK(__builtin_memcmp(ss1, ss2, KYBER_SSBYTES)           == 0);
    return 0;
}

// ---------------------------------------------------------------------------
// Test 5: different enc coins -> different shared secrets
// ---------------------------------------------------------------------------
static int test_different_coins_different_ss() {
    uint8_t pk[KYBER_PUBLICKEYBYTES] = {};
    uint8_t sk[KYBER_SECRETKEYBYTES] = {};
    uint8_t ct1[KYBER_CIPHERTEXTBYTES], ct2[KYBER_CIPHERTEXTBYTES];
    uint8_t ss1[KYBER_SSBYTES], ss2[KYBER_SSBYTES];
    uint8_t kp_coins[2 * KYBER_SYMBYTES] = {};
    uint8_t enc_coins_a[KYBER_SYMBYTES], enc_coins_b[KYBER_SYMBYTES];
    fill_seed(kp_coins,    sizeof(kp_coins),    0x42);
    fill_seed(enc_coins_a, sizeof(enc_coins_a), 0xFA);
    fill_seed(enc_coins_b, sizeof(enc_coins_b), 0xFB);

    CHECK(crypto_kem_keypair_derand(pk, sk, kp_coins)           == 0);
    CHECK(crypto_kem_enc_derand(ct1, ss1, pk, enc_coins_a)      == 0);
    CHECK(crypto_kem_enc_derand(ct2, ss2, pk, enc_coins_b)      == 0);
    CHECK(__builtin_memcmp(ss1, ss2, KYBER_SSBYTES) != 0);
    return 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    run_test("keypair_nonzero",              test_keypair_nonzero);
    run_test("encap_decap_roundtrip",        test_encap_decap_roundtrip);
    run_test("ciphertext_corruption",        test_ciphertext_corruption);
    run_test("determinism",                  test_determinism);
    run_test("different_coins_different_ss", test_different_coins_different_ss);

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}

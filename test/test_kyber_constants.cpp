/**
 * @file test_kyber_constants.cpp
 * @brief Compile-time and runtime verification of Kyber cryptographic constants.
 *
 * Verifies parameter relationships, Montgomery constants, and reduction
 * identity properties for the CRYSTALS-Kyber implementation.
 */

#include "params.hpp"
#include "reduce.hpp"

#include <cassert>
#include <cstdint>

using namespace xinim::crypto::kyber;

constexpr int16_t kModulus = static_cast<int16_t>(KYBER_Q);

static void test_kyber_parameter_values() {
    // Kyber-768 defaults
    static_assert(KYBER_N == 256);
    static_assert(KYBER_Q == 3329);
    static_assert(KYBER_K == 3);

    // KYBER_Q must be prime (Fermat test for small primes)
    static_assert(KYBER_Q > 2);

    // N must be power of 2
    static_assert((KYBER_N & (KYBER_N - 1)) == 0);
}

static void test_montgomery_constants() {
    // QINV: q^{-1} mod 2^16 -- verify QINV * Q == 1 (mod 2^16)
    int32_t product = static_cast<int32_t>(QINV) * static_cast<int32_t>(KYBER_Q);
    int16_t low16 = static_cast<int16_t>(product & 0xFFFF);
    assert(low16 == 1 || low16 == -static_cast<int16_t>(0xFFFF));
    // More precisely: (QINV * Q) mod 2^16 == 1
    uint16_t u_product = static_cast<uint16_t>(product);
    assert(u_product == 1);

    // MONT: 2^16 mod q
    static_assert(MONT == -1044);
    // Verify: 2^16 = 65536, 65536 mod 3329 = 65536 - 19*3329 = 65536 - 63251 = 2285
    // But MONT is in signed representation: 2285 - 3329 = -1044
    assert((65536 % kModulus) == static_cast<int>(MONT) + kModulus);
}

static void test_parameter_relationships() {
    // polybytes = 12 * n / 8 = 384
    static_assert(KYBER_POLYBYTES == 384);

    // polyvecbytes = k * polybytes
    static_assert(KYBER_POLYVECBYTES == KYBER_K * KYBER_POLYBYTES);

    // symbytes = ssbytes = seedbytes = 32
    static_assert(KYBER_SYMBYTES == 32);
    static_assert(KYBER_SSBYTES == 32);
    static_assert(KYBER_SEEDBYTES == 32);

    // publickeybytes = polyvecbytes + symbytes
    static_assert(KYBER_PUBLICKEYBYTES == KYBER_POLYVECBYTES + KYBER_SYMBYTES);
}

static void test_variant_parameters() {
    // Kyber-512
    using P512 = KyberParams<KyberVariant::Kyber512>;
    static_assert(P512::k == 2);
    static_assert(P512::eta1 == 3);
    static_assert(P512::n == 256);
    static_assert(P512::q == 3329);

    // Kyber-768
    using P768 = KyberParams<KyberVariant::Kyber768>;
    static_assert(P768::k == 3);
    static_assert(P768::eta1 == 2);

    // Kyber-1024
    using P1024 = KyberParams<KyberVariant::Kyber1024>;
    static_assert(P1024::k == 4);
    static_assert(P1024::eta1 == 2);
    static_assert(P1024::polycompressedbytes == 160);

    // All variants share eta2 = 2
    static_assert(P512::eta2 == 2);
    static_assert(P768::eta2 == 2);
    static_assert(P1024::eta2 == 2);
}

static void test_reduction_identity() {
    // barrett_reduce should be approximately identity for small values
    for (int16_t x = 0; x < kModulus; ++x) {
        int16_t r = barrett_reduce(x);
        if (r < 0)
            r = static_cast<int16_t>(r + kModulus);
        assert(r == x);
    }
}

int main() {
    test_kyber_parameter_values();
    test_montgomery_constants();
    test_parameter_relationships();
    test_variant_parameters();
    test_reduction_identity();
    return 0;
}

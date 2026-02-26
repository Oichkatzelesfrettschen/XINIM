/**
 * @file test_kyber_e2e.cpp
 * @brief Compile-time validation of Kyber512 constants and API declarations.
 *
 * WHY: MEMORY.md notes "Kyber full keygen/encap/decap: foundations correct but no
 *      end-to-end test". The kem/poly/polyvec/indcpa C source files are not yet
 *      present in kyber_impl/, so full keygen+encap+decap cannot be linked here.
 *      This test validates the Kyber512 constant values and API header correctness,
 *      establishing the compile-time gate for future full E2E wiring.
 *
 * TODO (Phase 7): Add kem.cpp, poly.cpp, polyvec.cpp, indcpa.cpp, symmetric.cpp,
 *      cbd.cpp, verify.cpp to kyber_impl/ and replace these stub assertions with a
 *      real keygen->encap->decap->verify shared-secret round-trip.
 */

#include "../crypto/kyber_impl/api.h"
#include "../crypto/kyber_impl/params.h"
#include <cassert>
#include <cstdint>

// Enforce Kyber512 for this test.
#ifndef KYBER_K
static_assert(false, "KYBER_K must be defined by the build system; expected 2 for Kyber512");
#endif
static_assert(KYBER_K == 2, "test_kyber_e2e expects KYBER_K=2 (Kyber512)");

static void test_kyber512_constants() {
    // api.h constants for pqcrystals_kyber512_ref
    static_assert(pqcrystals_kyber512_PUBLICKEYBYTES  == 800);
    static_assert(pqcrystals_kyber512_SECRETKEYBYTES  == 1632);
    static_assert(pqcrystals_kyber512_CIPHERTEXTBYTES == 768);
    static_assert(pqcrystals_kyber512_BYTES           == 32);

    // params.h constants must match api.h (with KYBER_K==2)
    static_assert(KYBER_PUBLICKEYBYTES  == pqcrystals_kyber512_PUBLICKEYBYTES);
    static_assert(KYBER_SECRETKEYBYTES  == pqcrystals_kyber512_SECRETKEYBYTES);
    static_assert(KYBER_CIPHERTEXTBYTES == pqcrystals_kyber512_CIPHERTEXTBYTES);
    static_assert(KYBER_SSBYTES         == pqcrystals_kyber512_BYTES);
}

static void test_kyber_core_parameters() {
    // Structural parameters that do not depend on K
    static_assert(KYBER_N == 256);
    static_assert(KYBER_Q == 3329);
    static_assert(KYBER_SSBYTES == 32);
    static_assert(KYBER_SYMBYTES == 32);

    // Kyber512-specific (K=2)
    static_assert(KYBER_K == 2);
    static_assert(KYBER_ETA1 == 3);
    static_assert(KYBER_ETA2 == 2);
    static_assert(KYBER_POLYBYTES == 384);
}

int main() {
    test_kyber512_constants();
    test_kyber_core_parameters();
    return 0;
}

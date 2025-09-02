/**
 * @file test_shared_secret_failure.cpp
 * @brief Unit tests exercising pqcrypto::compute_shared_secret error handling.
 */

#include "kyber.hpp"
#include "pqcrypto.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <random>

/**
 * @brief Validate that corrupting a public key yields a mismatched secret.
 *
 * The routine generates two key pairs, derives a shared secret as reference,
 * then flips one byte in the remote public key and confirms the derived secret
 * differs from the reference.
 *
 * @return Zero on success.
 */
int main() {
    // Generate Kyber key pairs for Alice and Bob
    auto alice_kp = pq::kyber::keypair();
    auto bob_kp = pq::kyber::keypair();

    // Convert keys to byte arrays accepted by pqcrypto
    std::array<std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> alice_pk{};
    std::array<std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> alice_sk{};
    std::array<std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> bob_pk{};
    std::array<std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> bob_sk{};

    std::transform(alice_kp.public_key.begin(), alice_kp.public_key.end(), alice_pk.begin(),
                   [](std::byte b) { return static_cast<std::uint8_t>(b); });
    std::transform(alice_kp.private_key.begin(), alice_kp.private_key.end(), alice_sk.begin(),
                   [](std::byte b) { return static_cast<std::uint8_t>(b); });
    std::transform(bob_kp.public_key.begin(), bob_kp.public_key.end(), bob_pk.begin(),
                   [](std::byte b) { return static_cast<std::uint8_t>(b); });
    std::transform(bob_kp.private_key.begin(), bob_kp.private_key.end(), bob_sk.begin(),
                   [](std::byte b) { return static_cast<std::uint8_t>(b); });

    // Establish a baseline shared secret between Alice and Bob
    auto reference = pqcrypto::compute_shared_secret(bob_pk, alice_sk);

    // Corrupt Bob's public key by flipping the first byte
    auto corrupted_pk = bob_pk;
    corrupted_pk[0] ^= 0xFFu;

    // Derive a secret using the corrupted key
    auto corrupted = pqcrypto::compute_shared_secret(corrupted_pk, alice_sk);

    // Verify the corrupted secret differs from the reference
    const bool mismatch = !std::equal(reference.begin(), reference.end(), corrupted.begin());
    assert(mismatch);
    return 0;
}

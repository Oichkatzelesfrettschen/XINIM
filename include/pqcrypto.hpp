#pragma once

#include "kyber_impl/api.h"
#include <array>
#include <cstdint>
#include <span>

namespace pqcrypto {

/**
 * @brief Key pair for lattice-based cryptography.
 */
struct KeyPair {
    std::array<std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> public_key; ///< Kyber public key
    std::array<std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> secret_key; ///< Kyber private key
};

/**
 * @brief Generate a new lattice-based key pair.
 *
 * The generated keys use the Kyber512 parameter set and are compatible
 * with the compute routines provided by this header.
 *
 * @return Newly created key pair containing Kyber public and private data.
 */
[[nodiscard]] KeyPair generate_keypair();

/**
 * @brief Derive a shared secret using key exchange.
 *
 * @param public_key Remote party public key.
 * @param secret_key Local private key.
 * @return Derived 32-byte shared secret.
 */
[[nodiscard]] std::array<std::uint8_t, pqcrystals_kyber512_BYTES>
compute_shared_secret(std::span<const std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> public_key,
                      std::span<const std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> secret_key);

} // namespace pqcrypto

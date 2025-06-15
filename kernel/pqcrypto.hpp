#pragma once
/**
 * @file pqcrypto.hpp
 * @brief Minimal post-quantum cryptography interface used by the lattice IPC layer.
 */
#include "../crypto/kyber_impl/api.h"
#include <array>
#include <cstdint>

namespace pqcrypto {

/**
 * @brief Simple key pair for establishing a shared secret.
 */
struct KeyPair {
    std::array<std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> public_key;  //!< Kyber public key
    std::array<std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> private_key; //!< Kyber private key
};

/**
 * @brief Generate a new post-quantum key pair.
 *
 * The generated key pair follows the Kyber512 specification used for
 * deriving channel secrets in the lattice IPC layer.
 *
 * @return Newly generated Kyber key pair.
 */
[[nodiscard]] KeyPair generate_keypair() noexcept;

/**
 * @brief Derive a shared secret from two key pairs.
 *
 * The secret is computed via Kyber512 encapsulation using @p peer 's public
 * key and decapsulation with @p local 's private key.
 *
 * @param local Local key pair owning the private component.
 * @param peer  Remote key pair providing the public component.
 * @return Derived 32-byte shared secret.
 */
[[nodiscard]] std::array<std::uint8_t, pqcrystals_kyber512_BYTES>
establish_secret(const KeyPair &local, const KeyPair &peer) noexcept;

} // namespace pqcrypto

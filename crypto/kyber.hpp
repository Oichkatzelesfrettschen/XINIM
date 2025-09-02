#pragma once

/**
 * @file kyber.hpp
 * @brief Kyber key encapsulation mechanism wrapper.
 *
 * The implementation
 * preferentially uses OpenSSL's KEM provider for Kyber512
 * when available and transparently falls
 * back to the bundled reference
 * implementation otherwise.
 */

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "kyber_impl/api.h"

namespace pq::kyber {

/**
 * @brief Public and private key pair used for encryption and decryption.
 */
struct KeyPair {
    std::array<std::byte, pqcrystals_kyber512_PUBLICKEYBYTES> public_key; ///< Generated public key
    std::array<std::byte, pqcrystals_kyber512_SECRETKEYBYTES>
        private_key; ///< Generated private key
};

/**
 * @brief Generate a new Kyber key pair.
 *
 * The returned key pair follows the Kyber512 parameter set and can be used
 * for encryption and decryption with this API.
 *
 * @return Generated key pair.
 */
[[nodiscard]] KeyPair keypair();

/**
 * @brief Encrypt a message with the given public key.
 *
 * The implementation performs a Kyber512 encapsulation to derive a shared
 * secret and then uses AES-256-GCM to encrypt the message. The resulting
 * ciphertext contains the Kyber encapsulation, AES nonce, authentication tag,
 * and encrypted payload.
 *
 * @param message    Plaintext input buffer.
 * @param public_key Public key to encrypt with.
 * @return Encrypted ciphertext buffer.
 */
[[nodiscard]] std::vector<std::byte>
encrypt(std::span<const std::byte> message,
        std::span<const std::byte, pqcrystals_kyber512_PUBLICKEYBYTES> public_key);

/**
 * @brief Decrypt a message with the given private key.
 *
 * Decryption extracts the Kyber encapsulation and AES parameters from the
 * ciphertext, derives the shared secret, and decrypts the payload using
 * AES-256-GCM.
 *
 * @param ciphertext Ciphertext input buffer.
 * @param private_key Private key to decrypt with.
 * @return Decrypted plaintext buffer.
 */
[[nodiscard]] std::vector<std::byte>
decrypt(std::span<const std::byte> ciphertext,
        std::span<const std::byte, pqcrystals_kyber512_SECRETKEYBYTES> private_key);

} // namespace pq::kyber

#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace pqcrypto {

/**
 * @brief Key pair for lattice-based cryptography.
 */
struct KeyPair {
    std::array<std::uint8_t, 32> public_key; ///< Public portion
    std::array<std::uint8_t, 32> secret_key; ///< Secret portion
};

/**
 * @brief Generate a new lattice-based key pair.
 *
 * This is a placeholder demonstrating the PQ interface.
 *
 * @return Newly created key pair.
 */
[[nodiscard]] KeyPair generate_keypair();

/**
 * @brief Derive a shared secret using key exchange.
 *
 * @param public_key Remote party public key.
 * @param secret_key Local secret key.
 * @return Derived shared secret bytes.
 */
[[nodiscard]] std::array<std::uint8_t, 32>
compute_shared_secret(std::span<const std::uint8_t, 32> public_key,
                      std::span<const std::uint8_t, 32> secret_key);

} // namespace pqcrypto

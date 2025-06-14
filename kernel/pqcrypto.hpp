#pragma once
/**
 * @file pqcrypto.hpp
 * @brief Minimal post-quantum cryptography interface used by the lattice IPC layer.
 */
#include <array>
#include <cstdint>

namespace pqcrypto {

/**
 * @brief Simple key pair for establishing a shared secret.
 */
struct KeyPair {
    std::array<std::uint8_t, 32> public_key;  //!< Public component
    std::array<std::uint8_t, 32> private_key; //!< Private component
};

/**
 * @brief Generate a new post-quantum key pair.
 *
 * The implementation uses a pseudo random number generator and does
 * not provide real security. It simply mimics an API expected by the
 * lattice IPC channel setup.
 *
 * @return Newly generated key pair.
 */
[[nodiscard]] KeyPair generate_keypair() noexcept;

/**
 * @brief Derive a shared secret from two key pairs.
 *
 * The operation XORs bytes from the caller's private key with the peer's
 * public key to produce a placeholder secret.
 *
 * @param local Local key pair.
 * @param peer Peer key pair.
 * @return Derived shared secret.
 */
[[nodiscard]] std::array<std::uint8_t, 32> establish_secret(const KeyPair &local,
                                                            const KeyPair &peer) noexcept;

} // namespace pqcrypto

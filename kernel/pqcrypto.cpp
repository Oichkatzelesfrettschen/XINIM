#include "pqcrypto.hpp"

/**
 * @file pqcrypto.cpp
 * @brief Kyber-based primitives for kernel key exchange.
 */

#include "../crypto/kyber_impl/api.h"
#include <array>
#include <cstdint>

namespace pqcrypto {

/// Generate a Kyber key pair for kernel use.
/// @return Newly created key pair.
KeyPair generate_keypair() noexcept {
    KeyPair kp{};
    pqcrystals_kyber512_ref_keypair(
        kp.public_key.data(),
        kp.private_key.data()
    );
    return kp;
}

/**
 * @brief Establish a shared secret via Kyber encapsulation.
 *
 * The routine encapsulates to @p peer using its public key and decapsulates
 * with the peer's private key to yield a symmetric secret. The @p local key
 * pair is reserved for future protocol extensions and is currently unused.
 *
 * @param local Local key pair (unused).
 * @param peer  Remote key pair providing the public component.
 * @return Derived shared secret.
 */
std::array<std::uint8_t, pqcrystals_kyber512_BYTES>
establish_secret(const KeyPair &local, const KeyPair &peer) noexcept {
    (void)local;  // local keypair currently unused
    std::array<std::uint8_t, pqcrystals_kyber512_BYTES> secret{};
    std::array<std::uint8_t, pqcrystals_kyber512_CIPHERTEXTBYTES> ct{};

    // Encapsulate to peer.public_key → ciphertext + shared secret
    pqcrystals_kyber512_ref_enc(
        ct.data(),
        secret.data(),
        peer.public_key.data()
    );

    // Decapsulate using peer.private_key to confirm shared secret
    pqcrystals_kyber512_ref_dec(
        secret.data(),
        ct.data(),
        peer.private_key.data()
    );

    return secret;
}

} // namespace pqcrypto

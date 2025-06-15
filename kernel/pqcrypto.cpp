#include "pqcrypto.hpp"

/**
 * @file pqcrypto.cpp
 * @brief Kyber-based primitives for kernel key exchange.
 */

#include "../crypto/kyber_impl/api.h"
#include <array>
#include <cstdint>
#include <span>

// Forward declaration of the span-based helper from the crypto library
namespace pqcrypto {
std::array<std::uint8_t, pqcrystals_kyber512_BYTES>
compute_shared_secret(std::span<const std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> public_key,
                      std::span<const std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> secret_key);
}

namespace pqcrypto {

/// Generate a Kyber key pair for kernel use.
/// @return Newly created key pair.
KeyPair generate_keypair() noexcept {
    KeyPair kp{};
    pqcrystals_kyber512_ref_keypair(kp.public_key.data(), kp.private_key.data());
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
std::array<std::uint8_t, pqcrystals_kyber512_BYTES> establish_secret(const KeyPair &local,
                                                                     const KeyPair &peer) noexcept {
    (void)local; // local keypair currently unused
    std::array<std::uint8_t, pqcrystals_kyber512_BYTES> secret{};
    std::array<std::uint8_t, pqcrystals_kyber512_CIPHERTEXTBYTES> ct{};

    // Encapsulate to peer.public_key → ciphertext + shared secret
    pqcrystals_kyber512_ref_enc(ct.data(), secret.data(), peer.public_key.data());

    // Decapsulate using peer.private_key to confirm shared secret
    pqcrystals_kyber512_ref_dec(secret.data(), ct.data(), peer.private_key.data());

    return secret;
}

/**
 * @brief Derive a shared secret given two key pairs.
 *
 * The helper validates the provided key sizes before forwarding to the
 * span-based implementation. On size mismatch an empty array is returned.
 */
std::array<std::uint8_t, pqcrystals_kyber512_BYTES>
compute_shared_secret(const KeyPair &local, const KeyPair &peer) noexcept {
    std::span<const std::uint8_t> pk{peer.public_key};
    std::span<const std::uint8_t> sk{local.private_key};

    if (pk.size() != pqcrystals_kyber512_PUBLICKEYBYTES ||
        sk.size() != pqcrystals_kyber512_SECRETKEYBYTES) {
        return {};
    }

    std::span<const std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> pk_fixed{peer.public_key};
    std::span<const std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> sk_fixed{local.private_key};
    return pqcrypto::compute_shared_secret(pk_fixed, sk_fixed);
}

} // namespace pqcrypto

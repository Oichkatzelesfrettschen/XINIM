/**
 * @file pqcrypto_shared.cpp
 * @brief Simple key exchange helper for deriving a shared secret.
 */

#include "pqcrypto.hpp"

#include "kyber_impl/api.h"
#include <array>
#include <cstdint>
#include <span>

namespace pqcrypto {

/**
 * @brief Compute a shared secret using the Kyber512 KEM.
 *
 * The routine performs a standard key encapsulation using the remote
 * party's public key and then decapsulates with the caller's private key
 * to derive the resulting session secret.
 *
 * @param public_key Remote party's public key span.
 * @param secret_key Local private key span.
 * @return Derived shared secret.
 */
[[nodiscard]] std::array<std::uint8_t, pqcrystals_kyber512_BYTES> compute_shared_secret(
    std::span<const std::uint8_t, pqcrystals_kyber512_PUBLICKEYBYTES> public_key,
    std::span<const std::uint8_t, pqcrystals_kyber512_SECRETKEYBYTES> secret_key) {
    std::array<std::uint8_t, pqcrystals_kyber512_BYTES> shared{};
    std::array<std::uint8_t, pqcrystals_kyber512_CIPHERTEXTBYTES> ct{};
    pqcrystals_kyber512_ref_enc(ct.data(), shared.data(), public_key.data());
    pqcrystals_kyber512_ref_dec(shared.data(), ct.data(), secret_key.data());
    return shared;
}

} // namespace pqcrypto

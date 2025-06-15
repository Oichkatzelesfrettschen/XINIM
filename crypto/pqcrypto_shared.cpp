/**
 * @file pqcrypto_shared.cpp
 * @brief Simple key exchange helper for deriving a shared secret.
 */

#include "pqcrypto.hpp"

#include <array>
#include <cstdint>
#include <ranges>
#include <span>

namespace pqcrypto {

/**
 * @brief Compute a shared secret using a naive XOR key exchange.
 *
 * This routine XORs the given public key with the provided secret key to
 * produce a 32-byte shared secret. The logic mirrors the kernel's
 * pqcrypto::establish_secret used during lattice IPC setup but lives in the
 * standalone crypto library.
 *
 * @param public_key Remote party's public key span.
 * @param secret_key Local secret key span.
 * @return Derived shared secret.
 */
[[nodiscard]] std::array<std::uint8_t, 32>
compute_shared_secret(std::span<const std::uint8_t, 32> public_key,
                      std::span<const std::uint8_t, 32> secret_key) {
    std::array<std::uint8_t, 32> secret{};
    std::ranges::for_each(std::views::iota(std::size_t{0}, secret.size()), [&](std::size_t i) {
        secret[i] = static_cast<std::uint8_t>(public_key[i] ^ secret_key[i]);
    });
    return secret;
}

} // namespace pqcrypto

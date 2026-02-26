#include "kyber.hpp"

/**
 * @file kyber.cpp
 * @brief Kyber512-based encryption implementation.
 *
 * This file uses the in-tree reference Kyber implementation and the vendored
 * FIPS 202 (Keccak) for hashing. No OpenSSL or libsodium dependencies.
 * Phase 7 will implement correct Keccak and NTT; until then these are stubs.
 */

// WHY: The encrypt/decrypt functions below use XOR with a truncated shared secret
// as a placeholder DEM. This is NOT authenticated encryption and provides NO
// message integrity or confidentiality guarantees. Replace with ChaCha20-Poly1305
// or AES-256-GCM before any use in production or security-sensitive code.
// See TODO Phase-7 comments in the function bodies.
#pragma message("SECURITY: kyber.cpp uses an insecure XOR placeholder DEM. See Phase 7.")

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

// Use vendored kernel RNG instead of libsodium
#include "vendored_sodium/randombytes_kernel.hpp"

namespace pq::kyber {

namespace {

/// Fill a byte array with random bytes using kernel RDRAND-based CSPRNG.
void random_bytes(std::span<std::byte> buffer) {
    kernel_randombytes(reinterpret_cast<unsigned char*>(buffer.data()), buffer.size());
}

} // namespace

KeyPair keypair() {
    KeyPair kp{};
    pqcrystals_kyber512_ref_keypair(reinterpret_cast<uint8_t *>(kp.public_key.data()),
                                    reinterpret_cast<uint8_t *>(kp.private_key.data()));
    return kp;
}

std::vector<std::byte>
encrypt(std::span<const std::byte> message,
        std::span<const std::byte, pqcrystals_kyber512_PUBLICKEYBYTES> public_key) {
    std::array<std::byte, pqcrystals_kyber512_CIPHERTEXTBYTES> kem_ct{};
    std::array<std::byte, pqcrystals_kyber512_BYTES> shared{};

    pqcrystals_kyber512_ref_enc(reinterpret_cast<uint8_t *>(kem_ct.data()),
                                reinterpret_cast<uint8_t *>(shared.data()),
                                reinterpret_cast<const uint8_t *>(public_key.data()));

    // TODO Phase-7: Implement proper AEAD (ChaCha20-Poly1305 or AES-GCM)
    // For now, XOR the message with the shared secret (NOT cryptographically secure)
    std::vector<std::byte> output;
    output.reserve(kem_ct.size() + message.size());
    output.insert(output.end(), kem_ct.begin(), kem_ct.end());
    for (std::size_t i = 0; i < message.size(); ++i) {
        output.push_back(message[i] ^ shared[i % shared.size()]);
    }
    return output;
}

std::vector<std::byte>
decrypt(std::span<const std::byte> ciphertext,
        std::span<const std::byte, pqcrystals_kyber512_SECRETKEYBYTES> private_key) {
    if (ciphertext.size() < pqcrystals_kyber512_CIPHERTEXTBYTES) {
        return {}; // ciphertext too short
    }

    std::array<std::byte, pqcrystals_kyber512_CIPHERTEXTBYTES> kem_ct{};
    std::copy_n(ciphertext.begin(), kem_ct.size(), kem_ct.begin());

    std::array<std::byte, pqcrystals_kyber512_BYTES> shared{};
    pqcrystals_kyber512_ref_dec(reinterpret_cast<uint8_t *>(shared.data()),
                                reinterpret_cast<const uint8_t *>(kem_ct.data()),
                                reinterpret_cast<const uint8_t *>(private_key.data()));

    // TODO Phase-7: Implement proper AEAD decryption
    auto payload = ciphertext.subspan(kem_ct.size());
    std::vector<std::byte> plain(payload.size());
    for (std::size_t i = 0; i < payload.size(); ++i) {
        plain[i] = payload[i] ^ shared[i % shared.size()];
    }
    return plain;
}

} // namespace pq::kyber

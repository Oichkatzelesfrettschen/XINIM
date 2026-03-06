/**
 * @file aes_hw.hpp
 * @brief AES-128 block cipher with AES-NI hardware acceleration.
 *
 * Provides key expansion and single-block encrypt/decrypt.
 * Runtime dispatch: uses AES-NI intrinsics when available,
 * falls back to portable bitslice implementation otherwise.
 *
 * Used by future ChaCha20-Poly1305 / AES-GCM AEAD layer and
 * kernel integrity checks.
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace xinim::crypto {

// AES-128 round key schedule (11 round keys, 176 bytes)
static constexpr size_t AES128_ROUNDS    = 10;
static constexpr size_t AES128_KEY_SIZE  = 16;
static constexpr size_t AES128_BLOCK_SIZE = 16;
static constexpr size_t AES128_EXPANDED_KEYS = AES128_ROUNDS + 1; // 11

struct Aes128Key {
    alignas(16) uint8_t round_keys[AES128_EXPANDED_KEYS * AES128_BLOCK_SIZE];
};

// Expand a 16-byte key into round keys
void aes128_expand_key(Aes128Key& ks, const uint8_t key[AES128_KEY_SIZE]);

// Encrypt a single 16-byte block in-place
void aes128_encrypt_block(uint8_t block[AES128_BLOCK_SIZE], const Aes128Key& ks);

// Decrypt a single 16-byte block in-place
void aes128_decrypt_block(uint8_t block[AES128_BLOCK_SIZE], const Aes128Key& ks);

} // namespace xinim::crypto

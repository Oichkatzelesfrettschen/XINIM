/**
 * @file sha2_hw.hpp
 * @brief SHA-256 with Intel SHA Extensions (SHA-NI) acceleration.
 *
 * Provides a simple one-shot sha256() and an incremental interface.
 * Runtime dispatch: uses SHA-NI intrinsics when available,
 * falls back to portable C++ implementation otherwise.
 *
 * NOT used by Kyber (which uses SHA-3 / SHAKE). Used by kernel
 * integrity checks and future TLS/X.509 support.
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace xinim::crypto {

static constexpr size_t SHA256_DIGEST_SIZE = 32;
static constexpr size_t SHA256_BLOCK_SIZE  = 64;

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t total_len;
    uint8_t  buf[SHA256_BLOCK_SIZE];
    size_t   buf_len;
};

void sha256_init(Sha256Ctx& ctx);
void sha256_update(Sha256Ctx& ctx, const uint8_t* data, size_t len);
void sha256_final(Sha256Ctx& ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

// One-shot convenience
void sha256(uint8_t digest[SHA256_DIGEST_SIZE], const uint8_t* data, size_t len);

} // namespace xinim::crypto

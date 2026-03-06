/**
 * @file sha2_hw.cpp
 * @brief SHA-256 with Intel SHA Extensions (SHA-NI) + software fallback.
 */

#include "sha2_hw.hpp"
#include <cstdlib>
#include <cstring>

#ifdef XINIM_ARCH_X86_64
#include "../kernel/arch/x86_64/cpu_features.hpp"
#endif

namespace xinim::crypto {

namespace {

// SHA-256 round constants
static constexpr uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

static constexpr uint32_t rotr(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

static constexpr uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static constexpr uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static constexpr uint32_t sigma0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static constexpr uint32_t sigma1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static constexpr uint32_t gamma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static constexpr uint32_t gamma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

static uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
           static_cast<uint32_t>(p[3]);
}

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >>  8);
    p[3] = static_cast<uint8_t>(v);
}

static void put_be64(uint8_t* p, uint64_t v) {
    put_be32(p, static_cast<uint32_t>(v >> 32));
    put_be32(p + 4, static_cast<uint32_t>(v));
}

// Software compress: process one 64-byte block
static void sw_compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = be32(block + i * 4);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = gamma1(w[i-2]) + w[i-7] + gamma0(w[i-15]) + w[i-16];
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = sigma0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

// ============================================================================
// SHA-NI hardware path
// ============================================================================

#if defined(__SHA__) && defined(__SSE4_1__)
#include <immintrin.h>

// SHA-NI compress: fully unrolled, follows Intel SHA Extensions whitepaper.
// state0 = ABEF (Intel convention), state1 = CDGH
// sha256rnds2(cdgh, abef, msg) updates cdgh; swap roles each half-round.
static void hw_compress(uint32_t state[8], const uint8_t block[64]) {
    __m128i abef_save, cdgh_save;
    __m128i state0, state1, msg, tmp;
    __m128i msg0, msg1, msg2, msg3;

    // Byte-swap mask for big-endian message words
    const __m128i shuf_mask = _mm_set_epi64x(
        static_cast<int64_t>(0x0c0d0e0f08090a0bULL),
        static_cast<int64_t>(0x0405060700010203ULL));

    // Load hash state: state[0..3] = ABCD, state[4..7] = EFGH
    // Intel SHA-NI wants state0 = FEBA (reversed pairs), state1 = HGDC
    __m128i tmp0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state));     // ABCD
    __m128i tmp1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state + 4)); // EFGH

    tmp  = _mm_shuffle_epi32(tmp0, 0xB1);  // BADC
    tmp1 = _mm_shuffle_epi32(tmp1, 0x1B);  // HGFE

    state0 = _mm_alignr_epi8(tmp, tmp1, 8);   // ABEF (Intel: FEBA)
    state1 = _mm_blend_epi16(tmp1, tmp, 0xF0); // CDGH (Intel: HGDC)

    abef_save = state0;
    cdgh_save = state1;

    // Helper macro: one round group (4 SHA-256 rounds)
    #define SHA256_ROUNDS(i, W) do { \
        msg = _mm_add_epi32(W, _mm_loadu_si128(reinterpret_cast<const __m128i*>(K + (i)*4))); \
        state1 = _mm_sha256rnds2_epu32(state1, state0, msg); \
        msg = _mm_shuffle_epi32(msg, 0x0E); \
        state0 = _mm_sha256rnds2_epu32(state0, state1, msg); \
    } while(0)

    // Rounds 0-3
    msg0 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(block +  0)), shuf_mask);
    SHA256_ROUNDS(0, msg0);

    // Rounds 4-7
    msg1 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 16)), shuf_mask);
    SHA256_ROUNDS(1, msg1);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);

    // Rounds 8-11
    msg2 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 32)), shuf_mask);
    SHA256_ROUNDS(2, msg2);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);

    // Rounds 12-15
    msg3 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 48)), shuf_mask);
    SHA256_ROUNDS(3, msg3);
    tmp = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);

    // Rounds 16-19
    SHA256_ROUNDS(4, msg0);
    tmp = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);

    // Rounds 20-23
    SHA256_ROUNDS(5, msg1);
    tmp = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);

    // Rounds 24-27
    SHA256_ROUNDS(6, msg2);
    tmp = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);

    // Rounds 28-31
    SHA256_ROUNDS(7, msg3);
    tmp = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);

    // Rounds 32-35
    SHA256_ROUNDS(8, msg0);
    tmp = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);

    // Rounds 36-39
    SHA256_ROUNDS(9, msg1);
    tmp = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);

    // Rounds 40-43
    SHA256_ROUNDS(10, msg2);
    tmp = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);

    // Rounds 44-47
    SHA256_ROUNDS(11, msg3);
    tmp = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);

    // Rounds 48-51
    SHA256_ROUNDS(12, msg0);
    tmp = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);

    // Rounds 52-55
    SHA256_ROUNDS(13, msg1);
    tmp = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);

    // Rounds 56-59
    SHA256_ROUNDS(14, msg2);
    tmp = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);

    // Rounds 60-63
    SHA256_ROUNDS(15, msg3);

    #undef SHA256_ROUNDS

    // Add saved state
    state0 = _mm_add_epi32(state0, abef_save);
    state1 = _mm_add_epi32(state1, cdgh_save);

    // Reverse the load shuffling to store back as state[ABCDEFGH].
    // Load did: state0 = alignr(shuffle(ABCD,0xB1), shuffle(EFGH,0x1B), 8)
    //           state1 = blend(shuffle(EFGH,0x1B), shuffle(ABCD,0xB1), 0xF0)
    // Inverse:
    tmp  = _mm_shuffle_epi32(state0, 0x1B);
    tmp1 = _mm_shuffle_epi32(state1, 0xB1);

    _mm_storeu_si128(reinterpret_cast<__m128i*>(state),
                     _mm_blend_epi16(tmp, tmp1, 0xF0));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(state + 4),
                     _mm_alignr_epi8(tmp1, tmp, 8));
}
#endif // __SHA__ && __SSE4_1__

// Dispatch compress
static void compress(uint32_t state[8], const uint8_t block[64]) {
#if defined(__SHA__) && defined(__SSE4_1__)
    if (xinim::arch::x86_64::g_cpu_features.sha_ni) {
        hw_compress(state, block);
        return;
    }
#endif
    sw_compress(state, block);
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

void sha256_init(Sha256Ctx& ctx) {
    ctx.state[0] = 0x6a09e667;
    ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372;
    ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f;
    ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab;
    ctx.state[7] = 0x5be0cd19;
    ctx.total_len = 0;
    ctx.buf_len = 0;
}

void sha256_update(Sha256Ctx& ctx, const uint8_t* data, size_t len) {
    ctx.total_len += len;

    // Fill partial buffer
    if (ctx.buf_len > 0) {
        size_t fill = SHA256_BLOCK_SIZE - ctx.buf_len;
        if (len < fill) {
            std::memcpy(ctx.buf + ctx.buf_len, data, len);
            ctx.buf_len += len;
            return;
        }
        std::memcpy(ctx.buf + ctx.buf_len, data, fill);
        compress(ctx.state, ctx.buf);
        data += fill;
        len -= fill;
        ctx.buf_len = 0;
    }

    // Process full blocks
    while (len >= SHA256_BLOCK_SIZE) {
        compress(ctx.state, data);
        data += SHA256_BLOCK_SIZE;
        len -= SHA256_BLOCK_SIZE;
    }

    // Save remainder
    if (len > 0) {
        std::memcpy(ctx.buf, data, len);
        ctx.buf_len = len;
    }
}

void sha256_final(Sha256Ctx& ctx, uint8_t digest[SHA256_DIGEST_SIZE]) {
    uint64_t total_bits = ctx.total_len * 8;

    // Pad: 0x80, zeros, 64-bit big-endian length
    uint8_t pad = static_cast<uint8_t>(SHA256_BLOCK_SIZE - ctx.buf_len);
    if (pad < 9) pad += static_cast<uint8_t>(SHA256_BLOCK_SIZE);

    uint8_t padding[128];
    std::memset(padding, 0, sizeof(padding));
    padding[0] = 0x80;
    put_be64(padding + pad - 8, total_bits);

    sha256_update(ctx, padding, pad);

    // Output digest
    for (int i = 0; i < 8; i++) {
        put_be32(digest + i * 4, ctx.state[i]);
    }
}

void sha256(uint8_t digest[SHA256_DIGEST_SIZE], const uint8_t* data, size_t len) {
    Sha256Ctx ctx;
    sha256_init(ctx);
    sha256_update(ctx, data, len);
    sha256_final(ctx, digest);
}

} // namespace xinim::crypto

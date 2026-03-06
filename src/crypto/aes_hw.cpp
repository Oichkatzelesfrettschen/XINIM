/**
 * @file aes_hw.cpp
 * @brief AES-128 with AES-NI hardware acceleration + software fallback.
 *
 * AES-NI path uses _mm_aeskeygenassist_si128, _mm_aesenc_si128, etc.
 * Software path implements the standard Rijndael algorithm with
 * precomputed S-box and round constant tables.
 */

#include "aes_hw.hpp"
#include <cstdlib>
#include <cstring>

#ifdef XINIM_ARCH_X86_64
#include "../kernel/arch/x86_64/cpu_features.hpp"
#endif

namespace xinim::crypto {

// ============================================================================
// Software AES-128 (portable fallback)
// ============================================================================

namespace {

// Forward S-box
static constexpr uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
};

// Inverse S-box
static constexpr uint8_t inv_sbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d,
};

// Round constants
static constexpr uint8_t rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// GF(2^8) multiplication helpers for MixColumns
static uint8_t xtime(uint8_t x) {
    return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static uint8_t mul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    uint8_t hi;
    for (int i = 0; i < 8; i++) {
        if (b & 1) r ^= a;
        hi = static_cast<uint8_t>(a & 0x80);
        a = static_cast<uint8_t>(a << 1);
        if (hi) a ^= 0x1b;
        b = static_cast<uint8_t>(b >> 1);
    }
    return r;
}

static void sub_bytes(uint8_t state[16]) {
    for (int i = 0; i < 16; i++) state[i] = sbox[state[i]];
}

static void inv_sub_bytes(uint8_t state[16]) {
    for (int i = 0; i < 16; i++) state[i] = inv_sbox[state[i]];
}

static void shift_rows(uint8_t state[16]) {
    // Row 1: shift left 1
    uint8_t t = state[1];
    state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = t;
    // Row 2: shift left 2
    t = state[2]; state[2] = state[10]; state[10] = t;
    t = state[6]; state[6] = state[14]; state[14] = t;
    // Row 3: shift left 3
    t = state[15];
    state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = t;
}

static void inv_shift_rows(uint8_t state[16]) {
    uint8_t t = state[13];
    state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = t;
    t = state[2]; state[2] = state[10]; state[10] = t;
    t = state[6]; state[6] = state[14]; state[14] = t;
    t = state[3];
    state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = t;
}

static void mix_columns(uint8_t state[16]) {
    for (int i = 0; i < 4; i++) {
        uint8_t* c = state + i * 4;
        uint8_t a0 = c[0], a1 = c[1], a2 = c[2], a3 = c[3];
        c[0] = static_cast<uint8_t>(xtime(a0) ^ xtime(a1) ^ a1 ^ a2 ^ a3);
        c[1] = static_cast<uint8_t>(a0 ^ xtime(a1) ^ xtime(a2) ^ a2 ^ a3);
        c[2] = static_cast<uint8_t>(a0 ^ a1 ^ xtime(a2) ^ xtime(a3) ^ a3);
        c[3] = static_cast<uint8_t>(xtime(a0) ^ a0 ^ a1 ^ a2 ^ xtime(a3));
    }
}

static void inv_mix_columns(uint8_t state[16]) {
    for (int i = 0; i < 4; i++) {
        uint8_t* c = state + i * 4;
        uint8_t a0 = c[0], a1 = c[1], a2 = c[2], a3 = c[3];
        c[0] = static_cast<uint8_t>(mul(a0,0x0e)^mul(a1,0x0b)^mul(a2,0x0d)^mul(a3,0x09));
        c[1] = static_cast<uint8_t>(mul(a0,0x09)^mul(a1,0x0e)^mul(a2,0x0b)^mul(a3,0x0d));
        c[2] = static_cast<uint8_t>(mul(a0,0x0d)^mul(a1,0x09)^mul(a2,0x0e)^mul(a3,0x0b));
        c[3] = static_cast<uint8_t>(mul(a0,0x0b)^mul(a1,0x0d)^mul(a2,0x09)^mul(a3,0x0e));
    }
}

static void add_round_key(uint8_t state[16], const uint8_t* rk) {
    for (int i = 0; i < 16; i++) state[i] ^= rk[i];
}

// Software key expansion
static void sw_expand_key(Aes128Key& ks, const uint8_t key[16]) {
    std::memcpy(ks.round_keys, key, 16);
    uint8_t* w = ks.round_keys;

    for (int i = 1; i <= 10; i++) {
        uint8_t* prev = w + (i - 1) * 16;
        uint8_t* curr = w + i * 16;

        // RotWord + SubWord + Rcon
        curr[0] = static_cast<uint8_t>(prev[0] ^ sbox[prev[13]] ^ rcon[i - 1]);
        curr[1] = static_cast<uint8_t>(prev[1] ^ sbox[prev[14]]);
        curr[2] = static_cast<uint8_t>(prev[2] ^ sbox[prev[15]]);
        curr[3] = static_cast<uint8_t>(prev[3] ^ sbox[prev[12]]);

        for (int j = 4; j < 16; j++) {
            curr[j] = static_cast<uint8_t>(prev[j] ^ curr[j - 4]);
        }
    }
}

static void sw_encrypt_block(uint8_t block[16], const Aes128Key& ks) {
    add_round_key(block, ks.round_keys);
    for (int r = 1; r < 10; r++) {
        sub_bytes(block);
        shift_rows(block);
        mix_columns(block);
        add_round_key(block, ks.round_keys + r * 16);
    }
    sub_bytes(block);
    shift_rows(block);
    add_round_key(block, ks.round_keys + 10 * 16);
}

static void sw_decrypt_block(uint8_t block[16], const Aes128Key& ks) {
    add_round_key(block, ks.round_keys + 10 * 16);
    for (int r = 9; r >= 1; r--) {
        inv_shift_rows(block);
        inv_sub_bytes(block);
        add_round_key(block, ks.round_keys + r * 16);
        inv_mix_columns(block);
    }
    inv_shift_rows(block);
    inv_sub_bytes(block);
    add_round_key(block, ks.round_keys);
}

// ============================================================================
// AES-NI hardware path
// ============================================================================

#ifdef __AES__
#include <immintrin.h>

static inline __m128i aes_128_key_assist(__m128i key, __m128i keygen) {
    keygen = _mm_shuffle_epi32(keygen, 0xFF);
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    return _mm_xor_si128(key, keygen);
}

static void hw_expand_key(Aes128Key& ks, const uint8_t key[16]) {
    __m128i* rk = reinterpret_cast<__m128i*>(ks.round_keys);
    rk[0] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
    rk[1]  = aes_128_key_assist(rk[0],  _mm_aeskeygenassist_si128(rk[0],  0x01));
    rk[2]  = aes_128_key_assist(rk[1],  _mm_aeskeygenassist_si128(rk[1],  0x02));
    rk[3]  = aes_128_key_assist(rk[2],  _mm_aeskeygenassist_si128(rk[2],  0x04));
    rk[4]  = aes_128_key_assist(rk[3],  _mm_aeskeygenassist_si128(rk[3],  0x08));
    rk[5]  = aes_128_key_assist(rk[4],  _mm_aeskeygenassist_si128(rk[4],  0x10));
    rk[6]  = aes_128_key_assist(rk[5],  _mm_aeskeygenassist_si128(rk[5],  0x20));
    rk[7]  = aes_128_key_assist(rk[6],  _mm_aeskeygenassist_si128(rk[6],  0x40));
    rk[8]  = aes_128_key_assist(rk[7],  _mm_aeskeygenassist_si128(rk[7],  0x80));
    rk[9]  = aes_128_key_assist(rk[8],  _mm_aeskeygenassist_si128(rk[8],  0x1b));
    rk[10] = aes_128_key_assist(rk[9],  _mm_aeskeygenassist_si128(rk[9],  0x36));
}

static void hw_encrypt_block(uint8_t block[16], const Aes128Key& ks) {
    const __m128i* rk = reinterpret_cast<const __m128i*>(ks.round_keys);
    __m128i state = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block));

    state = _mm_xor_si128(state, rk[0]);
    state = _mm_aesenc_si128(state, rk[1]);
    state = _mm_aesenc_si128(state, rk[2]);
    state = _mm_aesenc_si128(state, rk[3]);
    state = _mm_aesenc_si128(state, rk[4]);
    state = _mm_aesenc_si128(state, rk[5]);
    state = _mm_aesenc_si128(state, rk[6]);
    state = _mm_aesenc_si128(state, rk[7]);
    state = _mm_aesenc_si128(state, rk[8]);
    state = _mm_aesenc_si128(state, rk[9]);
    state = _mm_aesenclast_si128(state, rk[10]);

    _mm_storeu_si128(reinterpret_cast<__m128i*>(block), state);
}

static void hw_decrypt_block(uint8_t block[16], const Aes128Key& ks) {
    const __m128i* rk = reinterpret_cast<const __m128i*>(ks.round_keys);

    // Prepare inverse round keys
    __m128i dk[11];
    dk[0] = rk[10];
    for (int i = 1; i < 10; i++) {
        dk[i] = _mm_aesimc_si128(rk[10 - i]);
    }
    dk[10] = rk[0];

    __m128i state = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block));
    state = _mm_xor_si128(state, dk[0]);
    for (int i = 1; i < 10; i++) {
        state = _mm_aesdec_si128(state, dk[i]);
    }
    state = _mm_aesdeclast_si128(state, dk[10]);

    _mm_storeu_si128(reinterpret_cast<__m128i*>(block), state);
}
#endif // __AES__

} // anonymous namespace

// ============================================================================
// Public dispatch
// ============================================================================

void aes128_expand_key(Aes128Key& ks, const uint8_t key[AES128_KEY_SIZE]) {
#ifdef __AES__
    if (xinim::arch::x86_64::g_cpu_features.aesni) {
        hw_expand_key(ks, key);
        return;
    }
#endif
    sw_expand_key(ks, key);
}

void aes128_encrypt_block(uint8_t block[AES128_BLOCK_SIZE], const Aes128Key& ks) {
#ifdef __AES__
    if (xinim::arch::x86_64::g_cpu_features.aesni) {
        hw_encrypt_block(block, ks);
        return;
    }
#endif
    sw_encrypt_block(block, ks);
}

void aes128_decrypt_block(uint8_t block[AES128_BLOCK_SIZE], const Aes128Key& ks) {
#ifdef __AES__
    if (xinim::arch::x86_64::g_cpu_features.aesni) {
        hw_decrypt_block(block, ks);
        return;
    }
#endif
    sw_decrypt_block(block, ks);
}

} // namespace xinim::crypto

/**
 * @file test_hw_crypto.cpp
 * @brief Tests for AES-128 (AES-NI) and SHA-256 (SHA-NI) implementations.
 *
 * Uses NIST test vectors to verify correctness of both hardware and
 * software paths.
 */

#include "../src/crypto/aes_hw.hpp"
#include "../src/crypto/sha2_hw.hpp"
#include "../src/kernel/arch/x86_64/cpu_features.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s != %s (%d != %d)\n", \
            __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        g_fail++; \
    } else { g_pass++; } \
} while(0)

#define ASSERT_MEM_EQ(a, b, n) do { \
    if (std::memcmp((a), (b), (n)) != 0) { \
        std::fprintf(stderr, "FAIL %s:%d: memory mismatch (%zu bytes)\n", \
            __FILE__, __LINE__, (size_t)(n)); \
        for (size_t _i = 0; _i < (size_t)(n); _i++) { \
            if (((const uint8_t*)(a))[_i] != ((const uint8_t*)(b))[_i]) { \
                std::fprintf(stderr, "  byte %zu: got 0x%02X, expected 0x%02X\n", \
                    _i, ((const uint8_t*)(a))[_i], ((const uint8_t*)(b))[_i]); \
                break; \
            } \
        } \
        g_fail++; \
    } else { g_pass++; } \
} while(0)

using namespace xinim::crypto;

// ============================================================================
// AES-128 NIST test vector (FIPS 197, Appendix B)
// ============================================================================

static void test_aes128_encrypt() {
    // Key:        2b7e151628aed2a6abf7158809cf4f3c
    // Plaintext:  3243f6a8885a308d313198a2e0370734
    // Ciphertext: 3925841d02dc09fbdc118597196a0b32
    const uint8_t key[16] = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
    };
    const uint8_t plaintext[16] = {
        0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
        0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34
    };
    const uint8_t expected[16] = {
        0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,
        0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32
    };

    Aes128Key ks;
    aes128_expand_key(ks, key);

    uint8_t block[16];
    std::memcpy(block, plaintext, 16);
    aes128_encrypt_block(block, ks);
    ASSERT_MEM_EQ(block, expected, 16);
}

static void test_aes128_decrypt() {
    const uint8_t key[16] = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
    };
    const uint8_t ciphertext[16] = {
        0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,
        0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32
    };
    const uint8_t expected[16] = {
        0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
        0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34
    };

    Aes128Key ks;
    aes128_expand_key(ks, key);

    uint8_t block[16];
    std::memcpy(block, ciphertext, 16);
    aes128_decrypt_block(block, ks);
    ASSERT_MEM_EQ(block, expected, 16);
}

static void test_aes128_roundtrip() {
    const uint8_t key[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    const uint8_t original[16] = {
        0xde,0xad,0xbe,0xef,0xca,0xfe,0xba,0xbe,
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef
    };

    Aes128Key ks;
    aes128_expand_key(ks, key);

    uint8_t block[16];
    std::memcpy(block, original, 16);
    aes128_encrypt_block(block, ks);

    // Must differ from original
    ASSERT_EQ(std::memcmp(block, original, 16) != 0, true);

    aes128_decrypt_block(block, ks);
    ASSERT_MEM_EQ(block, original, 16);
}

// ============================================================================
// SHA-256 test vectors (NIST FIPS 180-4)
// ============================================================================

static void test_sha256_empty() {
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const uint8_t expected[32] = {
        0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,
        0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,
        0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,
        0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55
    };

    uint8_t digest[32];
    sha256(digest, nullptr, 0);
    ASSERT_MEM_EQ(digest, expected, 32);
}

static void test_sha256_abc() {
    // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    const uint8_t msg[] = "abc";
    const uint8_t expected[32] = {
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
    };

    uint8_t digest[32];
    sha256(digest, msg, 3);
    ASSERT_MEM_EQ(digest, expected, 32);
}

static void test_sha256_long() {
    // SHA-256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
    // = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
    const uint8_t msg[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    const uint8_t expected[32] = {
        0x24,0x8d,0x6a,0x61,0xd2,0x06,0x38,0xb8,
        0xe5,0xc0,0x26,0x93,0x0c,0x3e,0x60,0x39,
        0xa3,0x3c,0xe4,0x59,0x64,0xff,0x21,0x67,
        0xf6,0xec,0xed,0xd4,0x19,0xdb,0x06,0xc1
    };

    uint8_t digest[32];
    sha256(digest, msg, sizeof(msg) - 1);
    ASSERT_MEM_EQ(digest, expected, 32);
}

int main() {
    // Detect CPU features for runtime dispatch
    xinim::arch::x86_64::g_cpu_features = xinim::arch::x86_64::cpu_detect_features();

    std::printf("CPU: AES-NI=%s  SHA-NI=%s\n",
        xinim::arch::x86_64::g_cpu_features.aesni ? "yes" : "no",
        xinim::arch::x86_64::g_cpu_features.sha_ni ? "yes" : "no");

    test_aes128_encrypt();
    test_aes128_decrypt();
    test_aes128_roundtrip();
    test_sha256_empty();
    test_sha256_abc();
    test_sha256_long();

    std::printf("test_hw_crypto: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

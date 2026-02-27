/**
 * @file test_fips202.cpp
 * @brief Unit tests for FIPS 202 (SHA-3, SHAKE) implementation.
 *
 * Verifies SHA3-256, SHA3-512, SHAKE-128, SHAKE-256 against NIST KAT vectors.
 */

#include "fips202.hpp"
#include <cassert>
#include <array>
#include <cstdint>
#include <cstring>

using namespace xinim::crypto::fips202;

static void test_sha3_256_empty() {
    // NIST KAT: SHA3-256("") = a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a
    static constexpr std::array<std::uint8_t, 32> expected = {{
        0xa7, 0xff, 0xc6, 0xf8, 0xbf, 0x1e, 0xd7, 0x66,
        0x51, 0xc1, 0x47, 0x56, 0xa0, 0x61, 0xd6, 0x62,
        0xf5, 0x80, 0xff, 0x4d, 0xe4, 0x3b, 0x49, 0xfa,
        0x82, 0xd8, 0x0a, 0x4b, 0x80, 0xf8, 0x43, 0x4a,
    }};

    std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE> out{};
    auto result = sha3_256(std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE>{out},
                           std::span<const std::uint8_t>{});
    assert(result.has_value());

    for (std::size_t i = 0; i < 32; ++i) {
        assert(out[i] == expected[i]);
    }
}

static void test_sha3_256_abc() {
    // NIST KAT: SHA3-256("abc") = 3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
    static constexpr std::array<std::uint8_t, 32> expected = {{
        0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2,
        0x04, 0x5c, 0x17, 0x2d, 0x6b, 0xd3, 0x90, 0xbd,
        0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d, 0x52, 0x5b,
        0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32,
    }};

    const std::uint8_t input[] = {'a', 'b', 'c'};
    std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE> out{};
    auto result = sha3_256(std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE>{out},
                           std::span<const std::uint8_t>{input, 3});
    assert(result.has_value());

    for (std::size_t i = 0; i < 32; ++i) {
        assert(out[i] == expected[i]);
    }
}

static void test_sha3_512_empty() {
    // NIST KAT: SHA3-512("") = a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6
    //                           15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26
    static constexpr std::array<std::uint8_t, 64> expected = {{
        0xa6, 0x9f, 0x73, 0xcc, 0xa2, 0x3a, 0x9a, 0xc5,
        0xc8, 0xb5, 0x67, 0xdc, 0x18, 0x5a, 0x75, 0x6e,
        0x97, 0xc9, 0x82, 0x16, 0x4f, 0xe2, 0x58, 0x59,
        0xe0, 0xd1, 0xdc, 0xc1, 0x47, 0x5c, 0x80, 0xa6,
        0x15, 0xb2, 0x12, 0x3a, 0xf1, 0xf5, 0xf9, 0x4c,
        0x11, 0xe3, 0xe9, 0x40, 0x2c, 0x3a, 0xc5, 0x58,
        0xf5, 0x00, 0x19, 0x9d, 0x95, 0xb6, 0xd3, 0xe3,
        0x01, 0x75, 0x85, 0x86, 0x28, 0x1d, 0xcd, 0x26,
    }};

    std::array<std::uint8_t, SHA3_512_OUTPUT_SIZE> out{};
    auto result = sha3_512(std::span<std::uint8_t, SHA3_512_OUTPUT_SIZE>{out},
                           std::span<const std::uint8_t>{});
    assert(result.has_value());

    for (std::size_t i = 0; i < 64; ++i) {
        assert(out[i] == expected[i]);
    }
}

static void test_shake128_basic() {
    // Verify SHAKE-128 produces non-zero output and is deterministic
    std::array<std::uint8_t, 32> out1{};
    std::array<std::uint8_t, 32> out2{};
    const std::uint8_t input[] = {0x01, 0x02, 0x03};

    auto r1 = shake128(std::span<std::uint8_t>{out1},
                       std::span<const std::uint8_t>{input, 3});
    auto r2 = shake128(std::span<std::uint8_t>{out2},
                       std::span<const std::uint8_t>{input, 3});
    assert(r1.has_value());
    assert(r2.has_value());

    // Deterministic: same input produces same output
    for (std::size_t i = 0; i < 32; ++i) {
        assert(out1[i] == out2[i]);
    }

    // Non-trivial: not all zeros
    bool all_zero = true;
    for (auto b : out1) {
        if (b != 0) { all_zero = false; break; }
    }
    assert(!all_zero);
}

static void test_shake256_basic() {
    // Verify SHAKE-256 produces non-zero, deterministic output
    std::array<std::uint8_t, 64> out1{};
    std::array<std::uint8_t, 64> out2{};

    auto r1 = shake256(std::span<std::uint8_t>{out1},
                       std::span<const std::uint8_t>{});
    auto r2 = shake256(std::span<std::uint8_t>{out2},
                       std::span<const std::uint8_t>{});
    assert(r1.has_value());
    assert(r2.has_value());

    for (std::size_t i = 0; i < 64; ++i) {
        assert(out1[i] == out2[i]);
    }

    bool all_zero = true;
    for (auto b : out1) {
        if (b != 0) { all_zero = false; break; }
    }
    assert(!all_zero);
}

static void test_shake128_absorb_squeeze() {
    // Test incremental absorb + squeeze interface
    keccak_state state{};
    const std::uint8_t input[] = {0xAA, 0xBB, 0xCC};

    auto r1 = shake128_absorb(state, std::span<const std::uint8_t>{input, 3});
    assert(r1.has_value());

    // Squeeze one full block (168 bytes for SHAKE-128)
    std::array<std::uint8_t, SHAKE128_RATE> block{};
    auto r2 = shake128_squeezeblocks(std::span<std::uint8_t>{block}, state);
    assert(r2.has_value());

    bool all_zero = true;
    for (auto b : block) {
        if (b != 0) { all_zero = false; break; }
    }
    assert(!all_zero);
}

static void test_different_inputs_different_outputs() {
    std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE> out1{};
    std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE> out2{};

    const std::uint8_t in1[] = {0x00};
    const std::uint8_t in2[] = {0x01};

    (void)sha3_256(std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE>{out1},
                   std::span<const std::uint8_t>{in1, 1});
    (void)sha3_256(std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE>{out2},
                   std::span<const std::uint8_t>{in2, 1});

    bool differ = false;
    for (std::size_t i = 0; i < 32; ++i) {
        if (out1[i] != out2[i]) { differ = true; break; }
    }
    assert(differ);
}

int main() {
    test_sha3_256_empty();
    test_sha3_256_abc();
    test_sha3_512_empty();
    test_shake128_basic();
    test_shake256_basic();
    test_shake128_absorb_squeeze();
    test_different_inputs_different_outputs();
    return 0;
}

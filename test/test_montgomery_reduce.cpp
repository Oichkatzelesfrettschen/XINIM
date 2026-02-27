/**
 * @file test_montgomery_reduce.cpp
 * @brief Unit tests for Montgomery and Barrett reduction functions.
 *
 * Verifies correctness of modular reduction against known values.
 */

#include "reduce.hpp"
#include <cassert>
#include <cstdint>

using xinim::crypto::kyber::montgomery_reduce;
using xinim::crypto::kyber::barrett_reduce;
using xinim::crypto::kyber::csubq;
using xinim::crypto::kyber::KYBER_Q;
using xinim::crypto::kyber::QINV;

static void test_montgomery_basic() {
    // Montgomery reduction: given a (int32_t), computes a * 2^{-16} mod q
    // For a = KYBER_Q * 2^16, result should be 0 (mod q)
    int32_t a = static_cast<int32_t>(KYBER_Q) << 16;
    int16_t r = montgomery_reduce(a);
    // r should be congruent to 0 mod q
    int16_t canonical = barrett_reduce(r);
    if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
    assert(canonical == 0);
}

static void test_montgomery_identity() {
    // mont_reduce(x * 2^16) should give x mod q (for small x)
    for (int16_t x = 0; x < 100; ++x) {
        int32_t a = static_cast<int32_t>(x) << 16;
        int16_t r = montgomery_reduce(a);
        int16_t canonical = barrett_reduce(r);
        if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
        int16_t expected = static_cast<int16_t>(x % KYBER_Q);
        assert(canonical == expected);
    }
}

static void test_montgomery_negative() {
    // Test with negative inputs
    int32_t a = -static_cast<int32_t>(KYBER_Q) << 16;
    int16_t r = montgomery_reduce(a);
    int16_t canonical = barrett_reduce(r);
    if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
    assert(canonical == 0);
}

static void test_montgomery_product() {
    // mont_reduce(a * b) computes a * b * 2^{-16} mod q
    // If we compute mont_reduce(a * b) for a=1, b=2^16, we should get 1
    int32_t a = 1 * (1 << 16);
    int16_t r = montgomery_reduce(a);
    int16_t canonical = barrett_reduce(r);
    if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
    assert(canonical == 1);
}

static void test_barrett_basic() {
    // Barrett reduction: reduces a to range approximately [-q/2, q/2]
    // For values already in range, it should be approximately identity
    for (int16_t x = 0; x < 100; ++x) {
        int16_t r = barrett_reduce(x);
        // Result should be congruent to x mod q
        int16_t canonical = r;
        if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
        assert(canonical == x);
    }
}

static void test_barrett_large() {
    // For x >= q, barrett_reduce should bring it into range
    int16_t x = static_cast<int16_t>(KYBER_Q + 100);
    int16_t r = barrett_reduce(x);
    int16_t canonical = r;
    if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
    assert(canonical == 100);
}

static void test_barrett_negative() {
    // For negative values, should still reduce correctly
    int16_t x = static_cast<int16_t>(-100);
    int16_t r = barrett_reduce(x);
    int16_t canonical = r;
    if (canonical < 0) canonical = static_cast<int16_t>(canonical + KYBER_Q);
    assert(canonical == static_cast<int16_t>(KYBER_Q - 100));
}

static void test_csubq() {
    // csubq: conditional subtract q. If a >= q, subtract q.
    assert(csubq(0) == 0);
    assert(csubq(static_cast<int16_t>(KYBER_Q)) == 0);
    assert(csubq(static_cast<int16_t>(KYBER_Q - 1)) == static_cast<int16_t>(KYBER_Q - 1));
    assert(csubq(100) == 100);
}

static void test_constexpr_montgomery() {
    // Verify constexpr evaluation works
    static constexpr int16_t r = montgomery_reduce(static_cast<int32_t>(1) << 16);
    static_assert(r == 1 || r == 1 - KYBER_Q || r == 1 + KYBER_Q,
                  "constexpr montgomery_reduce(2^16) should yield 1 mod q");
}

int main() {
    test_montgomery_basic();
    test_montgomery_identity();
    test_montgomery_negative();
    test_montgomery_product();
    test_barrett_basic();
    test_barrett_large();
    test_barrett_negative();
    test_csubq();
    test_constexpr_montgomery();
    return 0;
}

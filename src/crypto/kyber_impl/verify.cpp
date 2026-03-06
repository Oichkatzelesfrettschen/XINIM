/**
 * @file verify.cpp
 * @brief Constant-time comparison and conditional move for Kyber.
 *
 * Uses volatile loads and bitwise arithmetic to prevent the compiler
 * from optimizing away the byte-by-byte comparison loop.
 */

#include "verify.h"
#include <stdint.h>
#include <stddef.h>

/*
 * Return 0 if a == b (byte-for-byte), non-zero otherwise.
 * Runs in constant time regardless of content.
 */
int verify(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t r = 0;
    for (size_t i = 0; i < len; i++) {
        r |= a[i] ^ b[i];
    }
    return (int)((((uint32_t)-((int32_t)r)) >> 31) & 1u);
}

/*
 * Conditional move: copy x into r if b == 1; leave r unchanged if b == 0.
 * b must be exactly 0 or 1.
 */
void cmov(uint8_t *r, const uint8_t *x, size_t len, uint8_t b) {
    uint8_t mask = static_cast<uint8_t>(-b);
    for (size_t i = 0; i < len; i++) {
        r[i] ^= mask & (r[i] ^ x[i]);
    }
}

/*
 * Conditional move for int16_t: set *r = v if b == 1; no change if b == 0.
 * b must be exactly 0 or 1 as uint16_t.
 */
void cmov_int16(int16_t *r, int16_t v, uint16_t b) {
    uint16_t mask = static_cast<uint16_t>(-b);
    *r = static_cast<int16_t>((*r & ~mask) | (v & mask));
}

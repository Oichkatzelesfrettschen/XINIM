/**
 * @file cbd.cpp
 * @brief Centered binomial distribution sampling for Kyber.
 *
 * Samples from CBD_eta using the bit-counting technique from
 * CRYSTALS-Kyber round 3 reference implementation.
 */

#include "cbd.h"
#include "params.h"
#include <stdint.h>

static uint32_t load32_littleendian(const uint8_t x[4]) {
    uint32_t r;
    r  = static_cast<uint32_t>(x[0]);
    r |= static_cast<uint32_t>(x[1]) << 8;
    r |= static_cast<uint32_t>(x[2]) << 16;
    r |= static_cast<uint32_t>(x[3]) << 24;
    return r;
}

#if KYBER_ETA1 == 3
static uint64_t load24_littleendian(const uint8_t x[3]) {
    uint64_t r;
    r  = static_cast<uint64_t>(x[0]);
    r |= static_cast<uint64_t>(x[1]) << 8;
    r |= static_cast<uint64_t>(x[2]) << 16;
    return r;
}
void poly_cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1 * KYBER_N / 4]) {
    uint32_t t, d;
    int16_t a, b;
    for (unsigned int i = 0; i < KYBER_N / 4; i++) {
        t  = static_cast<uint32_t>(load24_littleendian(buf + 3*i));
        d  = t & 0x00249249;
        d += (t >> 1) & 0x00249249;
        d += (t >> 2) & 0x00249249;
        for (unsigned int j = 0; j < 4; j++) {
            a = static_cast<int16_t>((d >>  (6*j+0)) & 0x7);
            b = static_cast<int16_t>((d >>  (6*j+3)) & 0x7);
            r->coeffs[4*i+j] = static_cast<int16_t>(a - b);
        }
    }
}
#elif KYBER_ETA1 == 2
void poly_cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1 * KYBER_N / 4]) {
    uint32_t t, d;
    int16_t a, b;
    for (unsigned int i = 0; i < KYBER_N / 8; i++) {
        t = load32_littleendian(buf + 4*i);
        d = t & 0x55555555;
        d += (t >> 1) & 0x55555555;
        for (unsigned int j = 0; j < 8; j++) {
            a = static_cast<int16_t>((d >> (4*j+0)) & 0x3);
            b = static_cast<int16_t>((d >> (4*j+2)) & 0x3);
            r->coeffs[8*i+j] = static_cast<int16_t>(a - b);
        }
    }
}
#endif

void poly_cbd_eta2(poly *r, const uint8_t buf[KYBER_ETA2 * KYBER_N / 4]) {
    uint32_t t, d;
    int16_t a, b;
    for (unsigned int i = 0; i < KYBER_N / 8; i++) {
        t = load32_littleendian(buf + 4*i);
        d = t & 0x55555555;
        d += (t >> 1) & 0x55555555;
        for (unsigned int j = 0; j < 8; j++) {
            a = static_cast<int16_t>((d >> (4*j+0)) & 0x3);
            b = static_cast<int16_t>((d >> (4*j+2)) & 0x3);
            r->coeffs[8*i+j] = static_cast<int16_t>(a - b);
        }
    }
}

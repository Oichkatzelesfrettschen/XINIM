/**
 * @file polyvec.cpp
 * @brief Kyber polynomial vector operations.
 *
 * Based on CRYSTALS-Kyber round 3 reference implementation.
 */

#include "polyvec.h"
#include "poly.h"
#include <stdint.h>

void polyvec_compress(uint8_t r[KYBER_POLYVECCOMPRESSEDBYTES], const polyvec *a) {
    unsigned int i, j, k;

#if (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 320))
    uint16_t t[4];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 4; j++) {
            for (k = 0; k < 4; k++) {
                t[k] = static_cast<uint16_t>(a->vec[i].coeffs[4*j+k]);
                t[k] += ((int16_t)t[k] >> 15) & KYBER_Q;
                t[k] = static_cast<uint16_t>(
                    ((static_cast<uint32_t>(t[k]) << 10) + KYBER_Q/2) / KYBER_Q) & 0x3FF;
            }
            r[0] = static_cast<uint8_t>(t[0] >> 0);
            r[1] = static_cast<uint8_t>((t[0] >> 8) | (t[1] << 2));
            r[2] = static_cast<uint8_t>((t[1] >> 6) | (t[2] << 4));
            r[3] = static_cast<uint8_t>((t[2] >> 4) | (t[3] << 6));
            r[4] = static_cast<uint8_t>(t[3] >> 2);
            r += 5;
        }
    }
#elif (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 352))
    uint16_t t[8];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 8; j++) {
            for (k = 0; k < 8; k++) {
                t[k] = static_cast<uint16_t>(a->vec[i].coeffs[8*j+k]);
                t[k] += ((int16_t)t[k] >> 15) & KYBER_Q;
                t[k] = static_cast<uint16_t>(
                    ((static_cast<uint32_t>(t[k]) << 11) + KYBER_Q/2) / KYBER_Q) & 0x7FF;
            }
            r[ 0] = static_cast<uint8_t>(t[0] >>  0);
            r[ 1] = static_cast<uint8_t>((t[0] >>  8) | (t[1] << 3));
            r[ 2] = static_cast<uint8_t>((t[1] >>  5) | (t[2] << 6));
            r[ 3] = static_cast<uint8_t>(t[2] >>  2);
            r[ 4] = static_cast<uint8_t>((t[2] >> 10) | (t[3] << 1));
            r[ 5] = static_cast<uint8_t>((t[3] >>  7) | (t[4] << 4));
            r[ 6] = static_cast<uint8_t>((t[4] >>  4) | (t[5] << 7));
            r[ 7] = static_cast<uint8_t>(t[5] >>  1);
            r[ 8] = static_cast<uint8_t>((t[5] >>  9) | (t[6] << 2));
            r[ 9] = static_cast<uint8_t>((t[6] >>  6) | (t[7] << 5));
            r[10] = static_cast<uint8_t>(t[7] >>  3);
            r += 11;
        }
    }
#endif
}

void polyvec_decompress(polyvec *r, const uint8_t a[KYBER_POLYVECCOMPRESSEDBYTES]) {
    unsigned int i, j, k;

#if (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 320))
    uint16_t t[4];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 4; j++) {
            t[0] = static_cast<uint16_t>((a[0] >> 0) | (static_cast<uint16_t>(a[1]) << 8));
            t[1] = static_cast<uint16_t>((a[1] >> 2) | (static_cast<uint16_t>(a[2]) << 6));
            t[2] = static_cast<uint16_t>((a[2] >> 4) | (static_cast<uint16_t>(a[3]) << 4));
            t[3] = static_cast<uint16_t>((a[3] >> 6) | (static_cast<uint16_t>(a[4]) << 2));
            a += 5;
            for (k = 0; k < 4; k++) {
                r->vec[i].coeffs[4*j+k] = static_cast<int16_t>(
                    (static_cast<uint32_t>(t[k] & 0x3FF) * KYBER_Q + 512) >> 10);
            }
        }
    }
#elif (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * 352))
    uint16_t t[8];
    for (i = 0; i < KYBER_K; i++) {
        for (j = 0; j < KYBER_N / 8; j++) {
            t[0] = static_cast<uint16_t>((a[ 0] >> 0) | (static_cast<uint16_t>(a[ 1]) << 8));
            t[1] = static_cast<uint16_t>((a[ 1] >> 3) | (static_cast<uint16_t>(a[ 2]) << 5));
            t[2] = static_cast<uint16_t>((a[ 2] >> 6) | (static_cast<uint16_t>(a[ 3]) << 2) | (static_cast<uint16_t>(a[4]) << 10));
            t[3] = static_cast<uint16_t>((a[ 4] >> 1) | (static_cast<uint16_t>(a[ 5]) << 7));
            t[4] = static_cast<uint16_t>((a[ 5] >> 4) | (static_cast<uint16_t>(a[ 6]) << 4));
            t[5] = static_cast<uint16_t>((a[ 6] >> 7) | (static_cast<uint16_t>(a[ 7]) << 1) | (static_cast<uint16_t>(a[8]) << 9));
            t[6] = static_cast<uint16_t>((a[ 8] >> 2) | (static_cast<uint16_t>(a[ 9]) << 6));
            t[7] = static_cast<uint16_t>((a[ 9] >> 5) | (static_cast<uint16_t>(a[10]) << 3));
            a += 11;
            for (k = 0; k < 8; k++) {
                r->vec[i].coeffs[8*j+k] = static_cast<int16_t>(
                    (static_cast<uint32_t>(t[k] & 0x7FF) * KYBER_Q + 1024) >> 11);
            }
        }
    }
#endif
}

void polyvec_tobytes(uint8_t r[KYBER_POLYVECBYTES], const polyvec *a) {
    for (unsigned int i = 0; i < KYBER_K; i++) {
        poly_tobytes(r + i * KYBER_POLYBYTES, &a->vec[i]);
    }
}

void polyvec_frombytes(polyvec *r, const uint8_t a[KYBER_POLYVECBYTES]) {
    for (unsigned int i = 0; i < KYBER_K; i++) {
        poly_frombytes(&r->vec[i], a + i * KYBER_POLYBYTES);
    }
}

void polyvec_ntt(polyvec *r) {
    for (unsigned int i = 0; i < KYBER_K; i++) {
        poly_ntt(&r->vec[i]);
    }
}

void polyvec_invntt_tomont(polyvec *r) {
    for (unsigned int i = 0; i < KYBER_K; i++) {
        poly_invntt_tomont(&r->vec[i]);
    }
}

void polyvec_basemul_acc_montgomery(poly *r, const polyvec *a, const polyvec *b) {
    poly t;
    poly_basemul_montgomery(r, &a->vec[0], &b->vec[0]);
    for (unsigned int i = 1; i < KYBER_K; i++) {
        poly_basemul_montgomery(&t, &a->vec[i], &b->vec[i]);
        poly_add(r, r, &t);
    }
    poly_reduce(r);
}

void polyvec_reduce(polyvec *r) {
    for (unsigned int i = 0; i < KYBER_K; i++) {
        poly_reduce(&r->vec[i]);
    }
}

void polyvec_add(polyvec *r, const polyvec *a, const polyvec *b) {
    for (unsigned int i = 0; i < KYBER_K; i++) {
        poly_add(&r->vec[i], &a->vec[i], &b->vec[i]);
    }
}

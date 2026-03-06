/**
 * @file poly.cpp
 * @brief Kyber polynomial operations.
 *
 * Based on the CRYSTALS-Kyber round 3 reference implementation.
 * Implements compress/decompress, byte packing, noise sampling,
 * NTT wrapper, and arithmetic on ring element polynomials.
 */

#include "poly.h"
#include "ntt.h"
#include "reduce.h"
#include "cbd.h"
#include "symmetric.h"
#include <stdint.h>


/*
 * Compression: map coefficients to [0, 2^d) for small d.
 * poly_compress: d = KYBER_POLYCOMPRESSEDBYTES * 8 / 256
 */

void poly_compress(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly *a) {
    uint8_t t[8];

#if (KYBER_POLYCOMPRESSEDBYTES == 128)
    for (unsigned int i = 0; i < KYBER_N / 8; i++) {
        for (unsigned int j = 0; j < 8; j++) {
            // Map to positive standard representatives
            int16_t u = a->coeffs[8*i+j];
            u += (u >> 15) & KYBER_Q;
            uint32_t d0 = static_cast<uint32_t>(u) << 4;
            d0 += 1665;
            d0 *= 80635;
            d0 >>= 28;
            t[j] = static_cast<uint8_t>(d0) & 15;
        }
        r[0] = t[0] | (t[1] << 4);
        r[1] = t[2] | (t[3] << 4);
        r[2] = t[4] | (t[5] << 4);
        r[3] = t[6] | (t[7] << 4);
        r += 4;
    }
#elif (KYBER_POLYCOMPRESSEDBYTES == 160)
    for (unsigned int i = 0; i < KYBER_N / 8; i++) {
        for (unsigned int j = 0; j < 8; j++) {
            // Map to positive standard representatives
            int16_t u = a->coeffs[8*i+j];
            u += (u >> 15) & KYBER_Q;
            uint32_t d0 = static_cast<uint32_t>(u) << 5;
            d0 += 1664;
            d0 *= 40318;
            d0 >>= 27;
            t[j] = static_cast<uint8_t>(d0) & 31;
        }
        r[0] = (t[0] >> 0) | (t[1] << 5);
        r[1] = (t[1] >> 3) | (t[2] << 2) | (t[3] << 7);
        r[2] = (t[3] >> 1) | (t[4] << 4);
        r[3] = (t[4] >> 4) | (t[5] << 1) | (t[6] << 6);
        r[4] = (t[6] >> 2) | (t[7] << 3);
        r += 5;
    }
#endif
}

void poly_decompress(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES]) {
#if (KYBER_POLYCOMPRESSEDBYTES == 128)
    for (unsigned int i = 0; i < KYBER_N / 2; i++) {
        r->coeffs[2*i+0] = static_cast<int16_t>((((uint16_t)(a[0] & 15) * KYBER_Q) + 8) >> 4);
        r->coeffs[2*i+1] = static_cast<int16_t>((((uint16_t)(a[0] >> 4) * KYBER_Q) + 8) >> 4);
        a += 1;
    }
#elif (KYBER_POLYCOMPRESSEDBYTES == 160)
    unsigned int i;
    uint8_t t[8];
    for (i = 0; i < KYBER_N / 8; i++) {
        t[0] = (a[0] >> 0);
        t[1] = (a[0] >> 5) | (a[1] << 3);
        t[2] = (a[1] >> 2);
        t[3] = (a[1] >> 7) | (a[2] << 1);
        t[4] = (a[2] >> 4) | (a[3] << 4);
        t[5] = (a[3] >> 1);
        t[6] = (a[3] >> 6) | (a[4] << 2);
        t[7] = (a[4] >> 3);
        a += 5;
        for (unsigned int j = 0; j < 8; j++) {
            r->coeffs[8*i+j] = static_cast<int16_t>(((uint32_t)(t[j] & 31) * KYBER_Q + 16) >> 5);
        }
    }
#endif
}

void poly_tobytes(uint8_t r[KYBER_POLYBYTES], const poly *a) {
    uint16_t t0, t1;
    for (unsigned int i = 0; i < KYBER_N / 2; i++) {
        t0 = static_cast<uint16_t>(a->coeffs[2*i]);
        t0 += ((int16_t)t0 >> 15) & KYBER_Q;
        t1 = static_cast<uint16_t>(a->coeffs[2*i+1]);
        t1 += ((int16_t)t1 >> 15) & KYBER_Q;
        r[3*i+0] = static_cast<uint8_t>(t0 >> 0);
        r[3*i+1] = static_cast<uint8_t>((t0 >> 8) | (t1 << 4));
        r[3*i+2] = static_cast<uint8_t>(t1 >> 4);
    }
}

void poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES]) {
    for (unsigned int i = 0; i < KYBER_N / 2; i++) {
        r->coeffs[2*i]   = static_cast<int16_t>(((a[3*i+0]      ) | (static_cast<uint16_t>(a[3*i+1]) << 8)) & 0xFFF);
        r->coeffs[2*i+1] = static_cast<int16_t>(((a[3*i+1] >> 4) | (static_cast<uint16_t>(a[3*i+2]) << 4)) & 0xFFF);
    }
}

void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES]) {
    for (unsigned int i = 0; i < KYBER_N / 8; i++) {
        for (unsigned int j = 0; j < 8; j++) {
            uint16_t mask = static_cast<uint16_t>(-((msg[i] >> j) & 1));
            r->coeffs[8*i+j] = static_cast<int16_t>(mask & ((KYBER_Q+1)/2));
        }
    }
}

void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *r) {
    uint16_t t;
    for (unsigned int i = 0; i < KYBER_N / 8; i++) {
        msg[i] = 0;
        for (unsigned int j = 0; j < 8; j++) {
            t = static_cast<uint16_t>(r->coeffs[8*i+j]);
            t += ((int16_t)t >> 15) & KYBER_Q;
            t  = (static_cast<uint16_t>(((t << 1) + KYBER_Q/2) / KYBER_Q) & 1);
            msg[i] |= static_cast<uint8_t>(t << j);
        }
    }
}

void poly_getnoise_eta1(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce) {
    uint8_t buf[KYBER_ETA1 * KYBER_N / 4];
    prf(buf, sizeof(buf), seed, nonce);
    poly_cbd_eta1(r, buf);
}

void poly_getnoise_eta2(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce) {
    uint8_t buf[KYBER_ETA2 * KYBER_N / 4];
    prf(buf, sizeof(buf), seed, nonce);
    poly_cbd_eta2(r, buf);
}

void poly_ntt(poly *r) {
    ntt(r->coeffs);
    poly_reduce(r);
}

void poly_invntt_tomont(poly *r) {
    invntt(r->coeffs);
}

void poly_basemul_montgomery(poly *r, const poly *a, const poly *b) {
    for (unsigned int i = 0; i < KYBER_N / 4; i++) {
        basemul(&r->coeffs[4*i],   &a->coeffs[4*i],   &b->coeffs[4*i],   zetas[64+i]);
        basemul(&r->coeffs[4*i+2], &a->coeffs[4*i+2], &b->coeffs[4*i+2], -zetas[64+i]);
    }
}

void poly_tomont(poly *r) {
    const int16_t f = static_cast<int16_t>((1ULL << 32) % KYBER_Q);
    for (unsigned int i = 0; i < KYBER_N; i++) {
        r->coeffs[i] = montgomery_reduce(static_cast<int32_t>(r->coeffs[i]) * f);
    }
}

void poly_reduce(poly *r) {
    for (unsigned int i = 0; i < KYBER_N; i++) {
        r->coeffs[i] = barrett_reduce(r->coeffs[i]);
    }
}

void poly_add(poly *r, const poly *a, const poly *b) {
    for (unsigned int i = 0; i < KYBER_N; i++) {
        r->coeffs[i] = static_cast<int16_t>(a->coeffs[i] + b->coeffs[i]);
    }
}

void poly_sub(poly *r, const poly *a, const poly *b) {
    for (unsigned int i = 0; i < KYBER_N; i++) {
        r->coeffs[i] = static_cast<int16_t>(a->coeffs[i] - b->coeffs[i]);
    }
}

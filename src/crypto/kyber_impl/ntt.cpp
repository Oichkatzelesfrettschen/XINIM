/**
 * @file ntt.cpp
 * @brief Number Theoretic Transform for Kyber polynomial multiplication.
 *
 * Implements NTT, inverse NTT, and base multiplication per the
 * CRYSTALS-Kyber reference implementation (round 3).
 * q = 3329, n = 256, primitive 512th root of unity = 17.
 */

#include "ntt.h"
#include "params.h"
#include "reduce.hpp"
#include "poly.h"

using xinim::crypto::kyber::montgomery_reduce;
using xinim::crypto::kyber::barrett_reduce;
using xinim::crypto::kyber::KYBER_Q;

/* Precomputed zetas in Montgomery domain (zeta_i * 2^16 mod q). */
const int16_t zetas[128] = {
    -1044,  -758,  -359, -1517,  1493,  1422,   287,   202,
     -171,   622,  1577,   182,   962, -1202, -1474,  1468,
      573, -1325,   264,   383,  -829,  1458, -1602,  -130,
     -681,  1017,   732,   608, -1542,   411,  -205, -1571,
     1223,   652,  -552,  1015, -1293,  1491,  -282, -1544,
      516,    -8,  -320,  -666, -1618, -1162,   126,  1469,
     -853,   -90,  -271,   830,   107, -1421,  -247,  -951,
     -398,   961, -1508,  -725,   448, -1065,   677, -1275,
    -1103,   430,   555,   843, -1251,   871,  1550,   105,
      422,   587,   177,  -235,  -291,  -460,  1574,  1653,
     -246,   778,  1159,  -147,  -777,  1483,  -602,  1119,
    -1590,   644,  -872,   349,   418,   329,  -156,   -75,
      817,  1097,   603,   610,  1322, -1285, -1465,   384,
    -1215,  -136,  1218, -1335,  -874,   220, -1187, -1659,
    -1185, -1530, -1278,   794, -1510,  -854,  -870,   478,
     -108,  -308,   996,   991,   958, -1460,  1522,  1628,
};

/**
 * @brief Forward NTT (in-place, Cooley-Tukey butterfly).
 *
 * Input coefficients in normal order, output in bit-reversed order.
 * All values remain in Montgomery domain.
 */
void ntt(int16_t r[256]) {
    unsigned int len, start, j, k;
    int16_t t, zeta;

    k = 1;
    for (len = 128; len >= 2; len >>= 1) {
        for (start = 0; start < 256; start = j + len) {
            zeta = zetas[k++];
            for (j = start; j < start + len; ++j) {
                t = static_cast<int16_t>(montgomery_reduce(
                    static_cast<int32_t>(zeta) * r[j + len]));
                r[j + len] = static_cast<int16_t>(r[j] - t);
                r[j] = static_cast<int16_t>(r[j] + t);
            }
        }
    }
}

/**
 * @brief Inverse NTT (in-place, Gentleman-Sande butterfly).
 *
 * Input in bit-reversed order, output in normal order.
 * Multiplies by Montgomery factor n^{-1} = 3303 (= 128^{-1} * 2^16 mod q).
 */
void invntt(int16_t r[256]) {
    unsigned int start, len, j, k;
    int16_t t, zeta;
    static constexpr int16_t f = 1441; /* 128^{-1} * 2^16 mod q */

    k = 127;
    for (len = 2; len <= 128; len <<= 1) {
        for (start = 0; start < 256; start = j + len) {
            zeta = zetas[k--];
            for (j = start; j < start + len; ++j) {
                t = r[j];
                r[j] = barrett_reduce(static_cast<int16_t>(t + r[j + len]));
                r[j + len] = static_cast<int16_t>(r[j + len] - t);
                r[j + len] = static_cast<int16_t>(montgomery_reduce(
                    static_cast<int32_t>(zeta) * r[j + len]));
            }
        }
    }

    for (j = 0; j < 256; ++j) {
        r[j] = static_cast<int16_t>(montgomery_reduce(
            static_cast<int32_t>(f) * r[j]));
    }
}

/**
 * @brief Multiplication of polynomials in NTT domain.
 *
 * Computes r = a * b in the base ring Z_q[X]/(X^2 - zeta).
 * Each call processes one degree-1 factor (2 coefficients).
 */
void basemul(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta) {
    r[0] = static_cast<int16_t>(montgomery_reduce(
        static_cast<int32_t>(a[1]) * b[1]));
    r[0] = static_cast<int16_t>(montgomery_reduce(
        static_cast<int32_t>(r[0]) * zeta));
    r[0] = static_cast<int16_t>(r[0] + montgomery_reduce(
        static_cast<int32_t>(a[0]) * b[0]));

    r[1] = static_cast<int16_t>(montgomery_reduce(
        static_cast<int32_t>(a[0]) * b[1]));
    r[1] = static_cast<int16_t>(r[1] + montgomery_reduce(
        static_cast<int32_t>(a[1]) * b[0]));
}

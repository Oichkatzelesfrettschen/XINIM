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
     -853,   -90, -1170,  1210,   334,  -536,  -112, -1623,
     -728,   -36,   842,  -500, -1024,  -211,   178,  1264,
      546,  -271,   732,  -596, -1425, -1455,  -225, -1400,
      -28,  -759, -1583,   -59, -1344,  1159,  -666,  -997,
      478,  -471,  -116, -1179,  1069,  1498,   213,   551,
     -150, -1270,   -71, -1201,  1023, -1494, -1269,   -48,
      648,   939,   -51, -1048, -1478,   149,  -764, -1217,
      -41, -1496,   574, -1020, -1040, -1140, -1260,  -627,
     1229, -1100, -1089,   752,   286,  1079,   484,  -956,
    -1292,  1508,   -19, -1200,   560, -1573, -1307, -1419,
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

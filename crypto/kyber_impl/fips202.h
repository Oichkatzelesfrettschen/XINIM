/**
 * @file fips202.h
 * @brief FIPS 202 header (SHA-3, SHAKE128, SHAKE256)
 * 
 * Header for FIPS 202 functions used by KYBER implementation.
 */

#ifndef FIPS202_H
#define FIPS202_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SHAKE128 extendable output function
 * @param out Output buffer
 * @param outlen Length of output
 * @param in Input buffer  
 * @param inlen Length of input
 */
void shake128(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);

/**
 * @brief SHAKE256 extendable output function
 * @param out Output buffer
 * @param outlen Length of output
 * @param in Input buffer
 * @param inlen Length of input
 */
void shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);

/**
 * @brief SHA3-256 hash function
 * @param out Output buffer (32 bytes)
 * @param in Input buffer
 * @param inlen Length of input
 */
void sha3_256(uint8_t *out, const uint8_t *in, size_t inlen);

/**
 * @brief SHA3-512 hash function
 * @param out Output buffer (64 bytes)
 * @param in Input buffer
 * @param inlen Length of input
 */
void sha3_512(uint8_t *out, const uint8_t *in, size_t inlen);

#ifdef __cplusplus
}
#endif

#endif /* FIPS202_H */
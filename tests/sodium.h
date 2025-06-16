#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sodium_init();
int crypto_aead_chacha20poly1305_ietf_encrypt(unsigned char *c, unsigned long long *clen,
                                              const unsigned char *m, unsigned long long mlen,
                                              const unsigned char *ad, unsigned long long adlen,
                                              const unsigned char *nsec, const unsigned char *npub,
                                              const unsigned char *k);
int crypto_aead_chacha20poly1305_ietf_decrypt(unsigned char *m, unsigned long long *mlen,
                                              unsigned char *nsec, const unsigned char *c,
                                              unsigned long long clen, const unsigned char *ad,
                                              unsigned long long adlen, const unsigned char *npub,
                                              const unsigned char *k);
void randombytes_buf(void *buf, size_t size);

#define crypto_aead_chacha20poly1305_ietf_ABYTES 16

#ifdef __cplusplus
}
#endif

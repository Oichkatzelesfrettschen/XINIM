#include "sodium.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>

extern "C" {

/**
 * @brief Initialize the sodium stub.
 *
 * This stub performs no real initialization and simply returns success.
 *
 * @return Always returns ``0``.
 */
int sodium_init() { return 0; }

/// Encrypt data by copying the plaintext and appending a zero tag.
int crypto_aead_chacha20poly1305_ietf_encrypt(unsigned char *c, unsigned long long *clen,
                                              const unsigned char *m, unsigned long long mlen,
                                              const unsigned char *, unsigned long long,
                                              const unsigned char *, const unsigned char *,
                                              const unsigned char *) {
    std::memcpy(c, m, mlen);
    std::memset(c + mlen, 0, crypto_aead_chacha20poly1305_ietf_ABYTES);
    *clen = mlen + crypto_aead_chacha20poly1305_ietf_ABYTES;
    return 0;
}

/// Decrypt data produced by the stub encrypt routine.
int crypto_aead_chacha20poly1305_ietf_decrypt(unsigned char *m, unsigned long long *mlen,
                                              unsigned char *, const unsigned char *c,
                                              unsigned long long clen, const unsigned char *,
                                              unsigned long long, const unsigned char *,
                                              const unsigned char *) {
    if (clen < crypto_aead_chacha20poly1305_ietf_ABYTES) {
        return -1;
    }
    *mlen = clen - crypto_aead_chacha20poly1305_ietf_ABYTES;
    std::memcpy(m, c, *mlen);
    return 0;
}

/// Fill a buffer with pseudo random bytes using ``std::random_device``.
void randombytes_buf(void *buf, size_t size) {
    static std::random_device rd;
    for (size_t i = 0; i < size; ++i) {
        static std::uniform_int_distribution<int> dist(0, 255);
        static std::mt19937 gen(rd());
        reinterpret_cast<unsigned char *>(buf)[i] = static_cast<unsigned char>(dist(gen));
    }
}

} // extern "C"

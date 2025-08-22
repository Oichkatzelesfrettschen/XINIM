#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <openssl/evp.h>
#include <openssl/sha.h>

/// \file crypt.cpp
/// \brief Password hashing using OpenSSL SHA-256.
///
/// Replaces the historical bespoke DES-like scheme with a modern
/// SHA-256 digest provided by OpenSSL. The resulting hash is returned as
/// a lowercase hexadecimal string.

/**
 * \brief Hash a password using a salt with SHA-256.
 *
 * The function computes `SHA256(salt || pw)` and returns the digest encoded
 * as a hexadecimal string. A static buffer is used for the output to maintain
 * compatibility with the traditional `crypt` interface.
 *
 * \param pw   Null-terminated password string.
 * \param salt Null-terminated salt string.
 * \return Pointer to a static buffer containing the hex-encoded digest.
 */
char *crypt(char *pw, char *salt) {
    static char buf[SHA256_DIGEST_LENGTH * 2 + 1];
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx{EVP_MD_CTX_new(), &EVP_MD_CTX_free};
    if (!ctx) {
        buf[0] = '\0';
        return buf;
    }

    EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx.get(), salt, std::strlen(salt));
    EVP_DigestUpdate(ctx.get(), pw, std::strlen(pw));
    EVP_DigestFinal_ex(ctx.get(), digest.data(), nullptr);

    for (size_t i = 0; i < digest.size(); ++i) {
        std::sprintf(buf + i * 2, "%02x", digest[i]);
    }
    buf[digest.size() * 2] = '\0';
    return buf;
}

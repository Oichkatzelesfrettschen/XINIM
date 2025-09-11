#include "kyber.hpp"

/**
 * @file kyber.cpp
 * @brief Kyber512-based encryption implementation.
 */

#include "../tests/sodium.hpp"
#include <openssl/core_names.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <stdexcept>

namespace pq::kyber {

namespace {

constexpr std::size_t NONCE_SIZE = 12;
constexpr std::size_t TAG_SIZE = 16;

/**
 * @brief Create a managed EVP_CIPHER_CTX.
 *
 * This helper allocates an OpenSSL cipher context and wraps it in a
 * `std::unique_ptr` with `EVP_CIPHER_CTX_free` as the deleter.
 *
 * @throws std::runtime_error if allocation fails.
 * @return Managed cipher context instance.
 */
[[nodiscard]] auto make_cipher_ctx()
    -> std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> {
    EVP_CIPHER_CTX *raw = EVP_CIPHER_CTX_new();
    if (!raw) {
        throw std::runtime_error{"EVP_CIPHER_CTX_new failed"};
    }
    return {raw, EVP_CIPHER_CTX_free};
}

/// Wrapper around OpenSSL AES-256-GCM encryption.
std::vector<std::byte> aes_gcm_encrypt(std::span<const std::byte> plain,
                                       std::span<const std::byte, pqcrystals_kyber512_BYTES> key,
                                       std::span<const std::byte, NONCE_SIZE> nonce,
                                       std::array<std::byte, TAG_SIZE> &tag) {
    auto ctx = make_cipher_ctx();

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        throw std::runtime_error{"EVP_EncryptInit_ex failed"};
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, NONCE_SIZE, nullptr) != 1) {
        throw std::runtime_error{"EVP_CTRL_GCM_SET_IVLEN failed"};
    }
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           reinterpret_cast<const unsigned char *>(nonce.data())) != 1) {
        throw std::runtime_error{"EVP_EncryptInit_ex key/nonce failed"};
    }

    std::vector<std::byte> cipher(plain.size());
    int out_len = 0;
    if (EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(cipher.data()), &out_len,
                          reinterpret_cast<const unsigned char *>(plain.data()),
                          static_cast<int>(plain.size())) != 1) {
        throw std::runtime_error{"EVP_EncryptUpdate failed"};
    }

    int tmp_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(cipher.data()) + out_len,
                            &tmp_len) != 1) {
        throw std::runtime_error{"EVP_EncryptFinal_ex failed"};
    }
    out_len += tmp_len;
    cipher.resize(out_len);

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1) {
        throw std::runtime_error{"EVP_CTRL_GCM_GET_TAG failed"};
    }
    return cipher;
}

/// Wrapper around OpenSSL AES-256-GCM decryption.
std::vector<std::byte> aes_gcm_decrypt(std::span<const std::byte> cipher,
                                       std::span<const std::byte, pqcrystals_kyber512_BYTES> key,
                                       std::span<const std::byte, NONCE_SIZE> nonce,
                                       std::span<const std::byte, TAG_SIZE> tag) {
    auto ctx = make_cipher_ctx();

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        throw std::runtime_error{"EVP_DecryptInit_ex failed"};
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, NONCE_SIZE, nullptr) != 1) {
        throw std::runtime_error{"EVP_CTRL_GCM_SET_IVLEN failed"};
    }
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           reinterpret_cast<const unsigned char *>(nonce.data())) != 1) {
        throw std::runtime_error{"EVP_DecryptInit_ex key/nonce failed"};
    }

    std::vector<std::byte> plain(cipher.size());
    int out_len = 0;
    if (EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(plain.data()), &out_len,
                          reinterpret_cast<const unsigned char *>(cipher.data()),
                          static_cast<int>(cipher.size())) != 1) {
        throw std::runtime_error{"EVP_DecryptUpdate failed"};
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
                            const_cast<std::byte *>(tag.data())) != 1) {
        throw std::runtime_error{"EVP_CTRL_GCM_SET_TAG failed"};
    }
    int tmp_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(plain.data()) + out_len,
                            &tmp_len) != 1) {
        throw std::runtime_error{"EVP_DecryptFinal_ex failed"};
    }
    out_len += tmp_len;
    plain.resize(out_len);
    return plain;
}

/// Encrypt data using libsodium's ChaCha20-Poly1305 AEAD.
[[maybe_unused]] std::vector<std::byte> sodium_aead_encrypt(
    std::span<const std::byte> plain, std::span<const std::byte, pqcrystals_kyber512_BYTES> key,
    std::span<const std::byte, NONCE_SIZE> nonce, std::array<std::byte, TAG_SIZE> &tag) {
    if (sodium_init() < 0) {
        throw std::runtime_error{"sodium_init failed"};
    }

    std::vector<std::byte> cipher(plain.size() + crypto_aead_chacha20poly1305_ietf_ABYTES);
    unsigned long long cipher_len = 0U;

    crypto_aead_chacha20poly1305_ietf_encrypt(
        reinterpret_cast<unsigned char *>(cipher.data()), &cipher_len,
        reinterpret_cast<const unsigned char *>(plain.data()), plain.size(), nullptr, 0, nullptr,
        reinterpret_cast<const unsigned char *>(nonce.data()),
        reinterpret_cast<const unsigned char *>(key.data()));

    std::span<const std::byte> tag_span{cipher.data() + plain.size(), TAG_SIZE};
    std::copy(tag_span.begin(), tag_span.end(), tag.begin());
    cipher.resize(static_cast<std::size_t>(cipher_len) - TAG_SIZE);
    return cipher;
}

/// Decrypt data using libsodium's ChaCha20-Poly1305 AEAD.
[[maybe_unused]] std::vector<std::byte> sodium_aead_decrypt(
    std::span<const std::byte> cipher, std::span<const std::byte, pqcrystals_kyber512_BYTES> key,
    std::span<const std::byte, NONCE_SIZE> nonce, std::span<const std::byte, TAG_SIZE> tag) {
    if (sodium_init() < 0) {
        throw std::runtime_error{"sodium_init failed"};
    }

    std::vector<std::byte> combined(cipher.size() + tag.size());
    std::copy(cipher.begin(), cipher.end(), combined.begin());
    std::copy(tag.begin(), tag.end(), combined.begin() + cipher.size());

    std::vector<std::byte> plain(cipher.size());
    unsigned long long plain_len = 0U;

    if (crypto_aead_chacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char *>(plain.data()), &plain_len, nullptr,
            reinterpret_cast<const unsigned char *>(combined.data()), combined.size(), nullptr, 0,
            reinterpret_cast<const unsigned char *>(nonce.data()),
            reinterpret_cast<const unsigned char *>(key.data())) != 0) {
        throw std::runtime_error{"sodium decrypt failed"};
    }

    plain.resize(static_cast<std::size_t>(plain_len));
    return plain;
}

/// Fill a byte array with cryptographically strong randomness using libsodium.
void random_bytes(std::span<std::byte> buffer) {
    if (sodium_init() < 0) {
        throw std::runtime_error{"sodium_init failed"};
    }
    randombytes_buf(buffer.data(), buffer.size());
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

/// Determine whether the current OpenSSL build exposes the Kyber KEM.
bool has_openssl_kyber() {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx{
        EVP_PKEY_CTX_new_from_name(nullptr, "KYBER512", nullptr), EVP_PKEY_CTX_free};
    return static_cast<bool>(ctx);
}

/// Generate a Kyber key pair via OpenSSL's KEM API.
KeyPair keypair_openssl() {
    KeyPair kp{};
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx{
        EVP_PKEY_CTX_new_from_name(nullptr, "KYBER512", nullptr), EVP_PKEY_CTX_free};
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0) {
        throw std::runtime_error{"EVP_PKEY_keygen_init failed"};
    }
    EVP_PKEY *raw = nullptr;
    if (EVP_PKEY_generate(ctx.get(), &raw) <= 0) {
        throw std::runtime_error{"EVP_PKEY_generate failed"};
    }
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey{raw, EVP_PKEY_free};

    size_t pk_len = kp.public_key.size();
    size_t sk_len = kp.private_key.size();
    if (EVP_PKEY_get_raw_public_key(
            pkey.get(), reinterpret_cast<unsigned char *>(kp.public_key.data()), &pk_len) <= 0 ||
        pk_len != kp.public_key.size()) {
        throw std::runtime_error{"EVP_PKEY_get_raw_public_key failed"};
    }
    if (EVP_PKEY_get_raw_private_key(
            pkey.get(), reinterpret_cast<unsigned char *>(kp.private_key.data()), &sk_len) <= 0 ||
        sk_len != kp.private_key.size()) {
        throw std::runtime_error{"EVP_PKEY_get_raw_private_key failed"};
    }
    return kp;
}

/// Perform Kyber encapsulation using OpenSSL to derive a shared secret.
void encapsulate_openssl(std::span<const std::byte, pqcrystals_kyber512_PUBLICKEYBYTES> pk,
                         std::array<std::byte, pqcrystals_kyber512_CIPHERTEXTBYTES> &ct,
                         std::array<std::byte, pqcrystals_kyber512_BYTES> &ss) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey{
        EVP_PKEY_new_raw_public_key_ex(nullptr, "KYBER512", nullptr,
                                       reinterpret_cast<const unsigned char *>(pk.data()),
                                       pk.size()),
        EVP_PKEY_free};
    if (!pkey) {
        throw std::runtime_error{"EVP_PKEY_new_raw_public_key_ex failed"};
    }
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx{
        EVP_PKEY_CTX_new_from_pkey(nullptr, pkey.get(), nullptr), EVP_PKEY_CTX_free};
    if (!ctx || EVP_PKEY_encapsulate_init(ctx.get(), nullptr) <= 0) {
        throw std::runtime_error{"EVP_PKEY_encapsulate_init failed"};
    }
    size_t ct_len = ct.size();
    size_t ss_len = ss.size();
    if (EVP_PKEY_encapsulate(ctx.get(), reinterpret_cast<unsigned char *>(ct.data()), &ct_len,
                             reinterpret_cast<unsigned char *>(ss.data()), &ss_len) <= 0 ||
        ct_len != ct.size() || ss_len != ss.size()) {
        throw std::runtime_error{"EVP_PKEY_encapsulate failed"};
    }
}

/// Perform Kyber decapsulation via OpenSSL.
void decapsulate_openssl(std::span<const std::byte, pqcrystals_kyber512_CIPHERTEXTBYTES> ct,
                         std::span<const std::byte, pqcrystals_kyber512_SECRETKEYBYTES> sk,
                         std::array<std::byte, pqcrystals_kyber512_BYTES> &ss) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey{
        EVP_PKEY_new_raw_private_key_ex(nullptr, "KYBER512", nullptr,
                                        reinterpret_cast<const unsigned char *>(sk.data()),
                                        sk.size()),
        EVP_PKEY_free};
    if (!pkey) {
        throw std::runtime_error{"EVP_PKEY_new_raw_private_key_ex failed"};
    }
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx{
        EVP_PKEY_CTX_new_from_pkey(nullptr, pkey.get(), nullptr), EVP_PKEY_CTX_free};
    if (!ctx || EVP_PKEY_decapsulate_init(ctx.get(), nullptr) <= 0) {
        throw std::runtime_error{"EVP_PKEY_decapsulate_init failed"};
    }
    size_t ss_len = ss.size();
    if (EVP_PKEY_decapsulate(ctx.get(), reinterpret_cast<unsigned char *>(ss.data()), &ss_len,
                             reinterpret_cast<const unsigned char *>(ct.data()), ct.size()) <= 0 ||
        ss_len != ss.size()) {
        throw std::runtime_error{"EVP_PKEY_decapsulate failed"};
    }
}

#else

bool has_openssl_kyber() { return false; }

#endif

} // namespace

KeyPair keypair() {
    if (has_openssl_kyber()) {
        try {
            return keypair_openssl();
        } catch (...) {
            // Fall back to reference implementation if OpenSSL fails.
        }
    }
    KeyPair kp{};
    pqcrystals_kyber512_ref_keypair(reinterpret_cast<uint8_t *>(kp.public_key.data()),
                                    reinterpret_cast<uint8_t *>(kp.private_key.data()));
    return kp;
}

std::vector<std::byte>
encrypt(std::span<const std::byte> message,
        std::span<const std::byte, pqcrystals_kyber512_PUBLICKEYBYTES> public_key) {
    std::array<std::byte, pqcrystals_kyber512_CIPHERTEXTBYTES> kem_ct{};
    std::array<std::byte, pqcrystals_kyber512_BYTES> shared{};

    if (has_openssl_kyber()) {
        encapsulate_openssl(public_key, kem_ct, shared);
    } else {
        pqcrystals_kyber512_ref_enc(reinterpret_cast<uint8_t *>(kem_ct.data()),
                                    reinterpret_cast<uint8_t *>(shared.data()),
                                    reinterpret_cast<const uint8_t *>(public_key.data()));
    }

    std::array<std::byte, NONCE_SIZE> nonce{};
    random_bytes(nonce);

    std::array<std::byte, TAG_SIZE> tag{};
    auto aes_cipher = aes_gcm_encrypt(message, shared, nonce, tag);

    std::vector<std::byte> output;
    output.reserve(kem_ct.size() + nonce.size() + tag.size() + aes_cipher.size());
    output.insert(output.end(), kem_ct.begin(), kem_ct.end());
    output.insert(output.end(), nonce.begin(), nonce.end());
    output.insert(output.end(), tag.begin(), tag.end());
    output.insert(output.end(), aes_cipher.begin(), aes_cipher.end());
    return output;
}

std::vector<std::byte>
decrypt(std::span<const std::byte> ciphertext,
        std::span<const std::byte, pqcrystals_kyber512_SECRETKEYBYTES> private_key) {
    if (ciphertext.size() < pqcrystals_kyber512_CIPHERTEXTBYTES + NONCE_SIZE + TAG_SIZE) {
        throw std::runtime_error{"ciphertext too short"};
    }

    std::array<std::byte, pqcrystals_kyber512_CIPHERTEXTBYTES> kem_ct{};
    std::copy_n(ciphertext.begin(), kem_ct.size(), kem_ct.begin());

    std::array<std::byte, NONCE_SIZE> nonce{};
    std::copy_n(ciphertext.begin() + kem_ct.size(), nonce.size(), nonce.begin());

    std::array<std::byte, TAG_SIZE> tag{};
    std::copy_n(ciphertext.begin() + kem_ct.size() + nonce.size(), tag.size(), tag.begin());

    auto enc_payload_begin = ciphertext.begin() + kem_ct.size() + nonce.size() + tag.size();
    std::span<const std::byte> enc_payload{enc_payload_begin, ciphertext.end()};

    std::array<std::byte, pqcrystals_kyber512_BYTES> shared{};
    if (has_openssl_kyber()) {
        decapsulate_openssl(kem_ct, private_key, shared);
    } else {
        pqcrystals_kyber512_ref_dec(reinterpret_cast<uint8_t *>(shared.data()),
                                    reinterpret_cast<const uint8_t *>(kem_ct.data()),
                                    reinterpret_cast<const uint8_t *>(private_key.data()));
    }

    return aes_gcm_decrypt(enc_payload, shared, nonce, tag);
}

} // namespace pq::kyber

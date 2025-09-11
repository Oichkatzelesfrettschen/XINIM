// Kyber parameters - C++23 constexpr definitions
#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::crypto::kyber {

// Kyber parameter sets
enum class KyberVariant {
    Kyber512,
    Kyber768,
    Kyber1024
};

// Template-based parameter selection for compile-time optimization
template<KyberVariant V>
struct KyberParams {
    static constexpr size_t k = (V == KyberVariant::Kyber512) ? 2 :
                                (V == KyberVariant::Kyber768) ? 3 : 4;
    static constexpr size_t n = 256;
    static constexpr size_t q = 3329;
    
    static constexpr size_t symbytes = 32;
    static constexpr size_t ssbytes = 32;
    static constexpr size_t seedbytes = 32;
    
    static constexpr size_t polybytes = 384;
    static constexpr size_t polyvecbytes = k * polybytes;
    static constexpr size_t polycompressedbytes = (V == KyberVariant::Kyber512 || 
                                                   V == KyberVariant::Kyber768) ? 128 : 160;
    static constexpr size_t polyveccompressedbytes = k * 320;
    
    static constexpr size_t eta1 = (V == KyberVariant::Kyber512) ? 3 : 2;
    static constexpr size_t eta2 = 2;
    
    static constexpr size_t publickeybytes = polyvecbytes + symbytes;
    static constexpr size_t secretkeybytes = polyvecbytes + publickeybytes + 2 * symbytes;
    static constexpr size_t ciphertextbytes = polyveccompressedbytes + polycompressedbytes;
    
    static constexpr size_t indcpa_msgbytes = symbytes;
    static constexpr size_t indcpa_publickeybytes = polyvecbytes + symbytes;
    static constexpr size_t indcpa_secretkeybytes = polyvecbytes;
    static constexpr size_t indcpa_bytes = polyveccompressedbytes + polycompressedbytes;
};

// Default to Kyber768 (recommended)
using DefaultParams = KyberParams<KyberVariant::Kyber768>;

// Legacy compatibility defines
constexpr size_t KYBER_N = DefaultParams::n;
constexpr size_t KYBER_Q = DefaultParams::q;
constexpr size_t KYBER_K = DefaultParams::k;

constexpr size_t KYBER_SYMBYTES = DefaultParams::symbytes;
constexpr size_t KYBER_SSBYTES = DefaultParams::ssbytes;
constexpr size_t KYBER_SEEDBYTES = DefaultParams::seedbytes;

constexpr size_t KYBER_POLYBYTES = DefaultParams::polybytes;
constexpr size_t KYBER_POLYVECBYTES = DefaultParams::polyvecbytes;
constexpr size_t KYBER_POLYCOMPRESSEDBYTES = DefaultParams::polycompressedbytes;
constexpr size_t KYBER_POLYVECCOMPRESSEDBYTES = DefaultParams::polyveccompressedbytes;

constexpr size_t KYBER_ETA1 = DefaultParams::eta1;
constexpr size_t KYBER_ETA2 = DefaultParams::eta2;

constexpr size_t KYBER_PUBLICKEYBYTES = DefaultParams::publickeybytes;
constexpr size_t KYBER_SECRETKEYBYTES = DefaultParams::secretkeybytes;
constexpr size_t KYBER_CIPHERTEXTBYTES = DefaultParams::ciphertextbytes;

constexpr size_t KYBER_INDCPA_MSGBYTES = DefaultParams::indcpa_msgbytes;
constexpr size_t KYBER_INDCPA_PUBLICKEYBYTES = DefaultParams::indcpa_publickeybytes;
constexpr size_t KYBER_INDCPA_SECRETKEYBYTES = DefaultParams::indcpa_secretkeybytes;
constexpr size_t KYBER_INDCPA_BYTES = DefaultParams::indcpa_bytes;

// Montgomery constants
constexpr int16_t MONT = -1044; // 2^16 mod q
constexpr int16_t QINV = -3327; // q^(-1) mod 2^16

} // namespace xinim::crypto::kyber

// C compatibility layer
#ifndef __cplusplus
    #define KYBER_N 256
    #define KYBER_Q 3329
    #define KYBER_K 3
    
    #define KYBER_SYMBYTES 32
    #define KYBER_SSBYTES 32
    #define KYBER_SEEDBYTES 32
    
    #define KYBER_POLYBYTES 384
    #define KYBER_POLYVECBYTES 1152
    #define KYBER_POLYCOMPRESSEDBYTES 128
    #define KYBER_POLYVECCOMPRESSEDBYTES 960
    
    #define KYBER_ETA1 2
    #define KYBER_ETA2 2
    
    #define KYBER_PUBLICKEYBYTES 1184
    #define KYBER_SECRETKEYBYTES 2400
    #define KYBER_CIPHERTEXTBYTES 1088
    
    #define KYBER_INDCPA_MSGBYTES 32
    #define KYBER_INDCPA_PUBLICKEYBYTES 1184
    #define KYBER_INDCPA_SECRETKEYBYTES 1152
    #define KYBER_INDCPA_BYTES 1088
    
    #define MONT -1044
    #define QINV -3327
#else
    using namespace xinim::crypto::kyber;
#endif
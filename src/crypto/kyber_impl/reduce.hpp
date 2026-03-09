// Modular reduction functions - C++23 with constexpr and SIMD
#pragma once

#include "params.hpp"
#include <cstdint>
#include <concepts>
#include <type_traits>

#if defined(__AVX2__) || defined(__x86_64__) || defined(__i386__)
    #include <immintrin.h>
#endif

namespace xinim::crypto::kyber {

// Montgomery reduction constant is now provided by params.hpp

// Modern C++23 constexpr Montgomery reduction
// IMPORTANT: KYBER_Q must be cast to int32_t to preserve signed arithmetic.
// params.hpp defines KYBER_Q as constexpr size_t; mixing with int32_t operands
// would produce unsigned arithmetic and wrong results for negative inputs.
template<std::integral T>
constexpr int16_t montgomery_reduce(T a) noexcept {
    static constexpr int32_t q = static_cast<int32_t>(KYBER_Q);
    int16_t t = static_cast<int16_t>(a) * QINV;
    t = static_cast<int16_t>((static_cast<int32_t>(a) - static_cast<int32_t>(t) * q) >> 16);
    return t;
}

// Barrett reduction with constexpr support
constexpr int16_t barrett_reduce(int16_t a) noexcept {
    constexpr int16_t v = ((1 << 26) + KYBER_Q / 2) / KYBER_Q;
    
    int16_t t = static_cast<int16_t>(((static_cast<int32_t>(v) * a + (1 << 25)) >> 26));
    t *= static_cast<int16_t>(KYBER_Q);
    return a - t;
}

// Conditional subtraction for constant-time reduction
constexpr int16_t csubq(int16_t a) noexcept {
    a -= static_cast<int16_t>(KYBER_Q);
    a += (a >> 15) & static_cast<int16_t>(KYBER_Q);
    return a;
}

#ifdef __AVX2__
// AVX2 vectorized Montgomery reduction for x86_64
inline __m256i montgomery_reduce_avx2(__m256i a) noexcept {
    const __m256i qinv_vec = _mm256_set1_epi16(QINV);
    const __m256i q_vec = _mm256_set1_epi16(KYBER_Q);
    
    __m256i t = _mm256_mullo_epi16(a, qinv_vec);
    __m256i high = _mm256_mulhi_epi16(t, q_vec);
    t = _mm256_mullo_epi16(t, q_vec);
    t = _mm256_sub_epi16(a, t);
    t = _mm256_srai_epi16(t, 15);
    
    return _mm256_sub_epi16(high, t);
}

// AVX2 vectorized Barrett reduction
inline __m256i barrett_reduce_avx2(__m256i a) noexcept {
    const __m256i v = _mm256_set1_epi16(20159);
    const __m256i q_vec = _mm256_set1_epi16(KYBER_Q);
    
    __m256i t = _mm256_mulhi_epi16(a, v);
    t = _mm256_add_epi16(t, a);
    t = _mm256_srai_epi16(t, 15);
    t = _mm256_mullo_epi16(t, q_vec);
    
    return _mm256_sub_epi16(a, t);
}
#endif

// Freeze coefficients to standard representatives
template<typename T>
requires std::integral<T> && std::signed_integral<T>
constexpr T freeze(T x) noexcept {
    return barrett_reduce(static_cast<int16_t>(x));
}

} // namespace xinim::crypto::kyber

// C-compatible interface
extern "C" {
    inline int16_t kyber_montgomery_reduce(int32_t a) {
        return xinim::crypto::kyber::montgomery_reduce(a);
    }
    
    inline int16_t kyber_barrett_reduce(int16_t a) {
        return xinim::crypto::kyber::barrett_reduce(a);
    }
}

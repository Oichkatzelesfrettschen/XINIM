// Modular reduction functions - C++23 with constexpr and SIMD
#pragma once

#include "params.hpp"
#include <xinim/hal/arch.hpp>
#include <cstdint>
#include <concepts>

#ifdef XINIM_ARCH_X86_64
    #include <immintrin.h>
#elif defined(XINIM_ARCH_ARM64)
    #include <arm_neon.h>
#endif

namespace xinim::crypto::kyber {

// Montgomery reduction constant
constexpr int16_t QINV = -3327; // q^(-1) mod 2^16

// Modern C++23 constexpr Montgomery reduction
template<std::integral T>
constexpr int16_t montgomery_reduce(T a) noexcept {
    if (std::is_constant_evaluated()) {
        // Compile-time evaluation
        int16_t t = static_cast<int16_t>(a) * QINV;
        t = static_cast<int16_t>((a - static_cast<int32_t>(t) * KYBER_Q) >> 16);
        return t;
    } else {
        // Runtime optimized version
        int16_t t = static_cast<int16_t>(a) * QINV;
        t = static_cast<int16_t>((a - static_cast<int32_t>(t) * KYBER_Q) >> 16);
        return t;
    }
}

// Barrett reduction with constexpr support
constexpr int16_t barrett_reduce(int16_t a) noexcept {
    constexpr int16_t v = ((1 << 26) + KYBER_Q / 2) / KYBER_Q;
    
    int16_t t = static_cast<int16_t>(((static_cast<int32_t>(v) * a + (1 << 25)) >> 26));
    t *= KYBER_Q;
    return a - t;
}

// Conditional subtraction for constant-time reduction
constexpr int16_t csubq(int16_t a) noexcept {
    a -= KYBER_Q;
    a += (a >> 15) & KYBER_Q;
    return a;
}

#ifdef XINIM_ARCH_X86_64
// AVX2 vectorized Montgomery reduction for x86_64
inline __m256i montgomery_reduce_avx2(__m256i a) noexcept {
    const __m256i qinv = _mm256_set1_epi16(QINV);
    const __m256i q = _mm256_set1_epi16(KYBER_Q);
    
    __m256i t = _mm256_mullo_epi16(a, qinv);
    __m256i high = _mm256_mulhi_epi16(t, q);
    t = _mm256_mullo_epi16(t, q);
    t = _mm256_sub_epi16(a, t);
    t = _mm256_srai_epi16(t, 15);
    
    return _mm256_sub_epi16(high, t);
}

// AVX2 vectorized Barrett reduction
inline __m256i barrett_reduce_avx2(__m256i a) noexcept {
    const __m256i v = _mm256_set1_epi16(20159);
    const __m256i q = _mm256_set1_epi16(KYBER_Q);
    
    __m256i t = _mm256_mulhi_epi16(a, v);
    t = _mm256_add_epi16(t, a);
    t = _mm256_srai_epi16(t, 15);
    t = _mm256_mullo_epi16(t, q);
    
    return _mm256_sub_epi16(a, t);
}
#endif

#ifdef XINIM_ARCH_ARM64
// NEON vectorized Montgomery reduction for ARM64
inline int16x8_t montgomery_reduce_neon(int32x4_t a_low, int32x4_t a_high) noexcept {
    const int16x8_t qinv = vdupq_n_s16(QINV);
    const int16x8_t q = vdupq_n_s16(KYBER_Q);
    
    // Combine high and low parts
    int16x4_t t_low = vmovn_s32(a_low);
    int16x4_t t_high = vmovn_s32(a_high);
    int16x8_t t = vcombine_s16(t_low, t_high);
    
    // Montgomery reduction
    t = vmulq_s16(t, qinv);
    int32x4_t prod_low = vmull_s16(vget_low_s16(t), vget_low_s16(q));
    int32x4_t prod_high = vmull_s16(vget_high_s16(t), vget_high_s16(q));
    
    a_low = vsubq_s32(a_low, prod_low);
    a_high = vsubq_s32(a_high, prod_high);
    
    t_low = vshrn_n_s32(a_low, 16);
    t_high = vshrn_n_s32(a_high, 16);
    
    return vcombine_s16(t_low, t_high);
}

// NEON vectorized Barrett reduction
inline int16x8_t barrett_reduce_neon(int16x8_t a) noexcept {
    const int16x8_t v = vdupq_n_s16(20159);
    const int16x8_t q = vdupq_n_s16(KYBER_Q);
    
    int16x8_t t = vqdmulhq_s16(a, v);
    t = vaddq_s16(t, a);
    t = vshrq_n_s16(t, 15);
    t = vmulq_s16(t, q);
    
    return vsubq_s16(a, t);
}
#endif

// Freeze coefficients to standard representatives
template<typename T>
requires std::integral<T> && std::signed_integral<T>
constexpr T freeze(T x) noexcept {
    return barrett_reduce(x);
}

} // namespace xinim::crypto::kyber

// C-compatible interface
extern "C" {
    inline int16_t montgomery_reduce(int32_t a) {
        return xinim::crypto::kyber::montgomery_reduce(a);
    }
    
    inline int16_t barrett_reduce(int16_t a) {
        return xinim::crypto::kyber::barrett_reduce(a);
    }
    
    inline int16_t csubq(int16_t a) {
        return xinim::crypto::kyber::csubq(a);
    }
}
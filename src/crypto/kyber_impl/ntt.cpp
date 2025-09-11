// Number Theoretic Transform (NTT) - C++23 with SIMD optimization
#include "ntt.hpp"
#include "reduce.hpp"
#include "params.hpp"
#include <xinim/hal/arch.hpp>
#include <xinim/hal/simd.hpp>
#include <array>
#include <bit>
#include <concepts>

namespace xinim::crypto::kyber {

// Precomputed zetas for NTT
constexpr std::array<int16_t, 128> zetas = {
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
     -108,  -308,   996,   991,   958, -1460,  1522,  1628
};

// Architecture-specific butterfly operations
namespace detail {

#ifdef XINIM_ARCH_X86_64
// AVX2 optimized butterfly for x86_64
inline void butterfly_avx2(int16_t* a, int16_t* b, int16_t zeta) noexcept {
    if (__builtin_cpu_supports("avx2")) {
        // AVX2 implementation for 16 coefficients at once
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b));
        __m256i vzeta = _mm256_set1_epi16(zeta);
        
        // t = fqmul(zeta, b[j])
        __m256i t = _mm256_mulhi_epi16(vb, vzeta);
        t = _mm256_mullo_epi16(t, _mm256_set1_epi16(KYBER_Q));
        t = _mm256_sub_epi16(_mm256_mullo_epi16(vb, vzeta), t);
        
        // b[j] = a[j] - t
        vb = _mm256_sub_epi16(va, t);
        // a[j] = a[j] + t
        va = _mm256_add_epi16(va, t);
        
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(a), va);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(b), vb);
        return;
    }
    // Fallback to scalar
}
#endif

#ifdef XINIM_ARCH_ARM64
// NEON optimized butterfly for ARM64
inline void butterfly_neon(int16_t* a, int16_t* b, int16_t zeta) noexcept {
    // NEON implementation for 8 coefficients at once
    int16x8_t va = vld1q_s16(a);
    int16x8_t vb = vld1q_s16(b);
    int16x8_t vzeta = vdupq_n_s16(zeta);
    
    // Montgomery multiplication approximation
    int32x4_t t_low = vmull_s16(vget_low_s16(vb), vget_low_s16(vzeta));
    int32x4_t t_high = vmull_s16(vget_high_s16(vb), vget_high_s16(vzeta));
    
    // Reduce
    int16x4_t t_reduced_low = vqrshrn_n_s32(t_low, 16);
    int16x4_t t_reduced_high = vqrshrn_n_s32(t_high, 16);
    int16x8_t t = vcombine_s16(t_reduced_low, t_reduced_high);
    
    // Butterfly operation
    vb = vsubq_s16(va, t);
    va = vaddq_s16(va, t);
    
    vst1q_s16(a, va);
    vst1q_s16(b, vb);
}
#endif

// Generic scalar butterfly
template<typename T>
requires std::integral<T> && std::signed_integral<T>
inline void butterfly_scalar(T& a, T& b, T zeta) noexcept {
    T t = montgomery_reduce(static_cast<int32_t>(zeta) * b);
    b = a - t;
    a = a + t;
}

} // namespace detail

// Main NTT function with architecture dispatch
void ntt(poly* p) noexcept {
    unsigned int len, start, j, k = 1;
    int16_t zeta;

    // Prefetch polynomial data for better cache usage
    xinim::hal::prefetch<xinim::hal::prefetch_hint::read_medium>(p->coeffs);
    
    // NTT layers
    for (len = 128; len >= 2; len >>= 1) {
        for (start = 0; start < 256; start += 2 * len) {
            zeta = zetas[k++];
            
            // Process butterflies
            for (j = start; j < start + len; ++j) {
#ifdef XINIM_ARCH_X86_64
                if constexpr (xinim::hal::is_x86_64) {
                    if (len >= 16 && __builtin_cpu_supports("avx2")) {
                        detail::butterfly_avx2(&p->coeffs[j], &p->coeffs[j + len], zeta);
                        j += 15; // Process 16 at once
                        continue;
                    }
                }
#endif
#ifdef XINIM_ARCH_ARM64
                if constexpr (xinim::hal::is_arm64) {
                    if (len >= 8) {
                        detail::butterfly_neon(&p->coeffs[j], &p->coeffs[j + len], zeta);
                        j += 7; // Process 8 at once
                        continue;
                    }
                }
#endif
                // Scalar fallback
                detail::butterfly_scalar(p->coeffs[j], p->coeffs[j + len], zeta);
            }
        }
    }
    
    // Memory barrier for consistency
    xinim::hal::memory_barrier();
}

// Inverse NTT with architecture optimization
void invntt(poly* p) noexcept {
    unsigned int start, len, j, k = 127;
    int16_t zeta;
    const int16_t f = 1441; // mont^2/128
    
    // Prefetch polynomial data
    xinim::hal::prefetch<xinim::hal::prefetch_hint::read_medium>(p->coeffs);
    
    // Inverse NTT layers
    for (len = 2; len <= 128; len <<= 1) {
        for (start = 0; start < 256; start += 2 * len) {
            zeta = -zetas[k--];
            
            for (j = start; j < start + len; ++j) {
#ifdef XINIM_ARCH_X86_64
                if constexpr (xinim::hal::is_x86_64) {
                    if (len >= 16 && __builtin_cpu_supports("avx2")) {
                        // AVX2 optimized inverse butterfly
                        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&p->coeffs[j]));
                        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&p->coeffs[j + len]));
                        
                        __m256i t = _mm256_add_epi16(va, vb);
                        vb = _mm256_sub_epi16(va, vb);
                        vb = _mm256_mullo_epi16(vb, _mm256_set1_epi16(zeta));
                        
                        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&p->coeffs[j]), t);
                        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&p->coeffs[j + len]), vb);
                        
                        j += 15;
                        continue;
                    }
                }
#endif
                // Scalar fallback
                int16_t t = p->coeffs[j];
                p->coeffs[j] = barrett_reduce(t + p->coeffs[j + len]);
                p->coeffs[j + len] = montgomery_reduce(static_cast<int32_t>(zeta) * (t - p->coeffs[j + len]));
            }
        }
    }
    
    // Final reduction
    for (j = 0; j < 256; ++j) {
        p->coeffs[j] = montgomery_reduce(static_cast<int32_t>(f) * p->coeffs[j]);
    }
    
    xinim::hal::memory_barrier();
}

// Multiplication of polynomials in NTT domain
void basemul(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta) noexcept {
    r[0] = montgomery_reduce(static_cast<int32_t>(a[1]) * b[1]);
    r[0] = montgomery_reduce(static_cast<int32_t>(r[0]) * zeta);
    r[0] += montgomery_reduce(static_cast<int32_t>(a[0]) * b[0]);
    
    r[1] = montgomery_reduce(static_cast<int32_t>(a[0]) * b[1]);
    r[1] += montgomery_reduce(static_cast<int32_t>(a[1]) * b[0]);
}

} // namespace xinim::crypto::kyber
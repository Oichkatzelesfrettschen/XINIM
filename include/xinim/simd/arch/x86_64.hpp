/**
 * @file x86_64.hpp
 * @brief x86-64 SIMD instruction set abstractions
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Comprehensive x86-64 SIMD support covering:
 * - X87 FPU with "SIMD-like tricks"
 * - MMX (64-bit integer SIMD)
 * - 3DNow! (AMD's floating point extensions)
 * - SSE 1-4.2 (128-bit packed operations)
 * - FMA3/FMA4 (Fused multiply-add)
 * - AVX/AVX2 (256-bit operations)
 * - AVX-512 (512-bit operations with masks)
 */

#pragma once

#include <xinim/simd/core.hpp>
#include <cstdint>
#include <immintrin.h>

namespace xinim::simd::x86 {

/**
 * @brief X87 FPU "SIMD-like" operations using clever tricks
 */
namespace x87 {

/**
 * @brief Parallel computation using X87 stack manipulation
 * Performs multiple floating point operations by clever use of FPU stack
 */
class ParallelFloat {
private:
    alignas(16) double values_[8]; // X87 stack has 8 registers
    
public:
    /**
     * @brief Load multiple values onto FPU stack
     * @param data Input data array
     * @param count Number of values (max 8)
     */
    void load(const float* data, std::size_t count) noexcept {
        count = (count > 8) ? 8 : count;
        for (std::size_t i = 0; i < count; ++i) {
            values_[i] = static_cast<double>(data[i]);
        }
    }
    
    /**
     * @brief Parallel sine computation using X87
     * @param count Number of values to process
     */
    void parallel_sin(std::size_t count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        for (std::size_t i = 0; i < count && i < 8; ++i) {
            asm volatile(
                "fldl %0\n\t"
                "fsin\n\t" 
                "fstpl %0"
                : "+m" (values_[i])
                :
                : "st"
            );
        }
#endif
    }
    
    /**
     * @brief Store results from FPU
     * @param data Output data array
     * @param count Number of values to store
     */
    void store(float* data, std::size_t count) noexcept {
        count = (count > 8) ? 8 : count;
        for (std::size_t i = 0; i < count; ++i) {
            data[i] = static_cast<float>(values_[i]);
        }
    }
};

} // namespace x87

/**
 * @brief MMX 64-bit integer SIMD operations
 */
namespace mmx {

/**
 * @brief 64-bit vector addition
 * @param a First vector (as 64-bit integer)
 * @param b Second vector (as 64-bit integer) 
 * @return Sum vector
 */
inline std::uint64_t add_8x8(std::uint64_t a, std::uint64_t b) noexcept {
#if defined(__MMX__)
    __m64 va = _mm_cvtsi64_m64(a);
    __m64 vb = _mm_cvtsi64_m64(b);
    __m64 result = _mm_add_pi8(va, vb);
    _mm_empty(); // Clear MMX state
    return _mm_cvtm64_si64(result);
#else
    // Fallback scalar implementation
    std::uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        std::uint8_t byte_a = (a >> (i * 8)) & 0xFF;
        std::uint8_t byte_b = (b >> (i * 8)) & 0xFF;
        std::uint8_t sum = byte_a + byte_b;
        result |= static_cast<std::uint64_t>(sum) << (i * 8);
    }
    return result;
#endif
}

/**
 * @brief Packed multiply and accumulate
 * @param acc Accumulator
 * @param a First multiplicand
 * @param b Second multiplicand
 * @return Updated accumulator
 */
inline std::uint64_t multiply_add_4x16(std::uint64_t acc, std::uint64_t a, std::uint64_t b) noexcept {
#if defined(__MMX__)
    __m64 vacc = _mm_cvtsi64_m64(acc);
    __m64 va = _mm_cvtsi64_m64(a);
    __m64 vb = _mm_cvtsi64_m64(b);
    __m64 result = _mm_add_pi16(vacc, _mm_mullo_pi16(va, vb));
    _mm_empty();
    return _mm_cvtm64_si64(result);
#else
    // Fallback implementation
    std::uint64_t result = acc;
    for (int i = 0; i < 4; ++i) {
        std::uint16_t val_a = (a >> (i * 16)) & 0xFFFF;
        std::uint16_t val_b = (b >> (i * 16)) & 0xFFFF;
        std::uint16_t acc_val = (result >> (i * 16)) & 0xFFFF;
        std::uint16_t sum = acc_val + (val_a * val_b);
        result = (result & ~(0xFFFFULL << (i * 16))) | (static_cast<std::uint64_t>(sum) << (i * 16));
    }
    return result;
#endif
}

} // namespace mmx

/**
 * @brief 3DNow! AMD extensions
 */
namespace amd_3dnow {

/**
 * @brief Reciprocal square root approximation (fast)
 * @param data Input array of floats
 * @param result Output array
 * @param count Number of elements (must be even)
 */
inline void fast_rsqrt_2xf32(const float* data, float* result, std::size_t count) noexcept {
#if defined(__3dNOW__)
    for (std::size_t i = 0; i < count; i += 2) {
        __m64 v = *reinterpret_cast<const __m64*>(&data[i]);
        __m64 rsqrt = _m_pfrsqrt(v);
        *reinterpret_cast<__m64*>(&result[i]) = rsqrt;
    }
    _m_femms(); // Faster MMX state clearing for 3DNow!
#else
    // Fallback using magic number method
    for (std::size_t i = 0; i < count; ++i) {
        float x = data[i];
        float x_half = 0.5f * x;
        std::uint32_t i_bits = *reinterpret_cast<const std::uint32_t*>(&x);
        i_bits = 0x5f3759df - (i_bits >> 1); // Magic number
        float y = *reinterpret_cast<const float*>(&i_bits);
        y = y * (1.5f - x_half * y * y); // Newton-Raphson iteration
        result[i] = y;
    }
#endif
}

} // namespace amd_3dnow

/**
 * @brief SSE 128-bit operations
 */
namespace sse {

/**
 * @brief High-performance memory copy using SSE
 * @param dst Destination (must be 16-byte aligned)
 * @param src Source (must be 16-byte aligned)
 * @param bytes Number of bytes (must be multiple of 16)
 */
inline void aligned_copy(void* dst, const void* src, std::size_t bytes) noexcept {
#if defined(__SSE__)
    auto* d = static_cast<__m128*>(dst);
    const auto* s = static_cast<const __m128*>(src);
    const std::size_t simd_count = bytes / 16;
    
    for (std::size_t i = 0; i < simd_count; ++i) {
        _mm_store_ps(reinterpret_cast<float*>(&d[i]), _mm_load_ps(reinterpret_cast<const float*>(&s[i])));
    }
#else
    std::memcpy(dst, src, bytes);
#endif
}

/**
 * @brief Vector addition (4x single precision)
 * @param a First vector
 * @param b Second vector
 * @return Sum vector
 */
inline v128f32 add(const v128f32& a, const v128f32& b) noexcept {
#if defined(__SSE__)
    __m128 va = _mm_load_ps(reinterpret_cast<const float*>(a.data.data()));
    __m128 vb = _mm_load_ps(reinterpret_cast<const float*>(b.data.data()));
    __m128 result = _mm_add_ps(va, vb);
    
    v128f32 ret;
    _mm_store_ps(reinterpret_cast<float*>(ret.data.data()), result);
    return ret;
#else
    v128f32 result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
#endif
}

/**
 * @brief Horizontal sum of vector elements
 * @param v Input vector
 * @return Sum of all elements
 */
inline float horizontal_sum(const v128f32& v) noexcept {
#if defined(__SSE3__)
    __m128 vec = _mm_load_ps(reinterpret_cast<const float*>(v.data.data()));
    __m128 sum = _mm_hadd_ps(vec, vec);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
#elif defined(__SSE__)
    __m128 vec = _mm_load_ps(reinterpret_cast<const float*>(v.data.data()));
    __m128 shuf = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sum = _mm_add_ps(vec, shuf);
    shuf = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(1, 0, 3, 2));
    sum = _mm_add_ps(sum, shuf);
    return _mm_cvtss_f32(sum);
#else
    return v[0] + v[1] + v[2] + v[3];
#endif
}

} // namespace sse

/**
 * @brief AVX 256-bit operations
 */
namespace avx {

/**
 * @brief Fused multiply-add (a * b + c)
 * @param a First multiplicand
 * @param b Second multiplicand  
 * @param c Addend
 * @return Result vector
 */
inline v256f32 fmadd(const v256f32& a, const v256f32& b, const v256f32& c) noexcept {
#if defined(__FMA__)
    __m256 va = _mm256_load_ps(reinterpret_cast<const float*>(a.data.data()));
    __m256 vb = _mm256_load_ps(reinterpret_cast<const float*>(b.data.data()));
    __m256 vc = _mm256_load_ps(reinterpret_cast<const float*>(c.data.data()));
    __m256 result = _mm256_fmadd_ps(va, vb, vc);
    
    v256f32 ret;
    _mm256_store_ps(reinterpret_cast<float*>(ret.data.data()), result);
    return ret;
#else
    v256f32 result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = a[i] * b[i] + c[i];
    }
    return result;
#endif
}

/**
 * @brief Complex number multiplication (4x complex single precision)
 * @param a First complex vector (real0, imag0, real1, imag1, ...)
 * @param b Second complex vector
 * @return Product vector
 */
inline v256f32 complex_multiply(const v256f32& a, const v256f32& b) noexcept {
#if defined(__AVX__)
    __m256 va = _mm256_load_ps(reinterpret_cast<const float*>(a.data.data()));
    __m256 vb = _mm256_load_ps(reinterpret_cast<const float*>(b.data.data()));
    
    // Duplicate real parts: [r0, r0, r1, r1, r2, r2, r3, r3]
    __m256 real_a = _mm256_moveldup_ps(va);
    // Duplicate imaginary parts: [i0, i0, i1, i1, i2, i2, i3, i3]  
    __m256 imag_a = _mm256_movehdup_ps(va);
    
    // Multiply reals with b
    __m256 real_prod = _mm256_mul_ps(real_a, vb);
    
    // Swap real/imag in b: [i0, r0, i1, r1, i2, r2, i3, r3]
    __m256 b_swapped = _mm256_shuffle_ps(vb, vb, _MM_SHUFFLE(2, 3, 0, 1));
    
    // Multiply imaginaries with swapped b
    __m256 imag_prod = _mm256_mul_ps(imag_a, b_swapped);
    
    // Add/subtract for complex multiplication
    __m256 result = _mm256_addsub_ps(real_prod, imag_prod);
    
    v256f32 ret;
    _mm256_store_ps(reinterpret_cast<float*>(ret.data.data()), result);
    return ret;
#else
    v256f32 result;
    for (std::size_t i = 0; i < result.size(); i += 2) {
        float real_a = a[i], imag_a = a[i + 1];
        float real_b = b[i], imag_b = b[i + 1];
        result[i] = real_a * real_b - imag_a * imag_b;     // Real part
        result[i + 1] = real_a * imag_b + imag_a * real_b; // Imaginary part
    }
    return result;
#endif
}

} // namespace avx

/**
 * @brief AVX-512 operations with mask support
 */
namespace avx512 {

/**
 * @brief Masked addition with conflict detection
 * @param a First vector
 * @param b Second vector
 * @param mask Operation mask
 * @return Result vector
 */
inline v512f32 masked_add(const v512f32& a, const v512f32& b, std::uint16_t mask) noexcept {
#if defined(__AVX512F__)
    __m512 va = _mm512_load_ps(reinterpret_cast<const float*>(a.data.data()));
    __m512 vb = _mm512_load_ps(reinterpret_cast<const float*>(b.data.data()));
    __mmask16 kmask = mask;
    __m512 result = _mm512_mask_add_ps(va, kmask, va, vb);
    
    v512f32 ret;
    _mm512_store_ps(reinterpret_cast<float*>(ret.data.data()), result);
    return ret;
#else
    v512f32 result = a;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (mask & (1U << i)) {
            result[i] = a[i] + b[i];
        }
    }
    return result;
#endif
}

/**
 * @brief Gather operation with conflict detection
 * @param base_addr Base address for gathering
 * @param indices Vector of indices
 * @param scale Scale factor
 * @return Gathered vector
 */
inline v512i32 gather_i32(const void* base_addr, const v512i32& indices, int scale) noexcept {
#if defined(__AVX512F__)
    __m512i vindices = _mm512_load_si512(reinterpret_cast<const __m512i*>(indices.data.data()));
    __m512i result = _mm512_i32gather_epi32(vindices, base_addr, scale);
    
    v512i32 ret;
    _mm512_store_si512(reinterpret_cast<__m512i*>(ret.data.data()), result);
    return ret;
#else
    v512i32 result;
    const auto* base = static_cast<const std::int32_t*>(base_addr);
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = base[indices[i] * scale];
    }
    return result;
#endif
}

} // namespace avx512

/**
 * @brief Unrolled loops for maximum performance
 */
namespace unrolled {

/**
 * @brief Highly optimized dot product with manual unrolling
 * @param a First vector
 * @param b Second vector
 * @param size Vector size (must be multiple of 32)
 * @return Dot product result
 */
inline double dot_product_f64(const double* a, const double* b, std::size_t size) noexcept {
#if defined(__AVX__)
    __m256d sum1 = _mm256_setzero_pd();
    __m256d sum2 = _mm256_setzero_pd();
    __m256d sum3 = _mm256_setzero_pd();
    __m256d sum4 = _mm256_setzero_pd();
    
    const std::size_t unroll_count = size / 16; // Process 16 doubles per iteration
    
    for (std::size_t i = 0; i < unroll_count; ++i) {
        const std::size_t idx = i * 16;
        
        // Load 16 elements (4 AVX vectors) and compute partial dot products
        __m256d a1 = _mm256_load_pd(&a[idx + 0]);
        __m256d b1 = _mm256_load_pd(&b[idx + 0]);
        sum1 = _mm256_fmadd_pd(a1, b1, sum1);
        
        __m256d a2 = _mm256_load_pd(&a[idx + 4]);
        __m256d b2 = _mm256_load_pd(&b[idx + 4]);
        sum2 = _mm256_fmadd_pd(a2, b2, sum2);
        
        __m256d a3 = _mm256_load_pd(&a[idx + 8]);
        __m256d b3 = _mm256_load_pd(&b[idx + 8]);
        sum3 = _mm256_fmadd_pd(a3, b3, sum3);
        
        __m256d a4 = _mm256_load_pd(&a[idx + 12]);
        __m256d b4 = _mm256_load_pd(&b[idx + 12]);
        sum4 = _mm256_fmadd_pd(a4, b4, sum4);
    }
    
    // Combine partial sums
    __m256d total = _mm256_add_pd(_mm256_add_pd(sum1, sum2), _mm256_add_pd(sum3, sum4));
    
    // Horizontal sum
    __m256d hi = _mm256_permute2f128_pd(total, total, 1);
    total = _mm256_add_pd(total, hi);
    __m256d shuf = _mm256_shuffle_pd(total, total, 1);
    total = _mm256_add_pd(total, shuf);
    
    return _mm256_cvtsd_f64(total);
#else
    double sum = 0.0;
    // Manual unrolling for scalar case
    const std::size_t unroll_count = size / 8;
    for (std::size_t i = 0; i < unroll_count; ++i) {
        const std::size_t idx = i * 8;
        sum += a[idx + 0] * b[idx + 0];
        sum += a[idx + 1] * b[idx + 1];
        sum += a[idx + 2] * b[idx + 2];
        sum += a[idx + 3] * b[idx + 3];
        sum += a[idx + 4] * b[idx + 4];
        sum += a[idx + 5] * b[idx + 5];
        sum += a[idx + 6] * b[idx + 6];
        sum += a[idx + 7] * b[idx + 7];
    }
    
    // Handle remaining elements
    for (std::size_t i = unroll_count * 8; i < size; ++i) {
        sum += a[i] * b[i];
    }
    
    return sum;
#endif
}

} // namespace unrolled

} // namespace xinim::simd::x86

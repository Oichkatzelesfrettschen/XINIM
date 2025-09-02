// XINIM Hardware Abstraction Layer - SIMD Operations
#pragma once

#include "arch.hpp"
#include <cstdint>
#include <array>
#include <span>

#ifdef XINIM_ARCH_X86_64
    #include <immintrin.h>
#elif defined(XINIM_ARCH_ARM64)
    #include <arm_neon.h>
#endif

namespace xinim::hal::simd {

// Unified 128-bit vector type
struct alignas(16) vec128 {
    union {
#ifdef XINIM_ARCH_X86_64
        __m128i xmm;
#elif defined(XINIM_ARCH_ARM64)
        uint8x16_t neon;
        uint32x4_t neon32;
        uint64x2_t neon64;
#endif
        std::array<uint8_t, 16> bytes;
        std::array<uint32_t, 4> u32;
        std::array<uint64_t, 2> u64;
    };

    // Constructors
    vec128() = default;
    
#ifdef XINIM_ARCH_X86_64
    vec128(__m128i v) : xmm(v) {}
    operator __m128i() const { return xmm; }
#elif defined(XINIM_ARCH_ARM64)
    vec128(uint8x16_t v) : neon(v) {}
    operator uint8x16_t() const { return neon; }
#endif
};

// Load operations
inline vec128 load_aligned(const void* ptr) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    result.xmm = _mm_load_si128(static_cast<const __m128i*>(ptr));
#elif defined(XINIM_ARCH_ARM64)
    result.neon = vld1q_u8(static_cast<const uint8_t*>(ptr));
#endif
    return result;
}

inline vec128 load_unaligned(const void* ptr) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    result.xmm = _mm_loadu_si128(static_cast<const __m128i*>(ptr));
#elif defined(XINIM_ARCH_ARM64)
    result.neon = vld1q_u8(static_cast<const uint8_t*>(ptr));
#endif
    return result;
}

// Store operations
inline void store_aligned(void* ptr, vec128 v) noexcept {
#ifdef XINIM_ARCH_X86_64
    _mm_store_si128(static_cast<__m128i*>(ptr), v.xmm);
#elif defined(XINIM_ARCH_ARM64)
    vst1q_u8(static_cast<uint8_t*>(ptr), v.neon);
#endif
}

inline void store_unaligned(void* ptr, vec128 v) noexcept {
#ifdef XINIM_ARCH_X86_64
    _mm_storeu_si128(static_cast<__m128i*>(ptr), v.xmm);
#elif defined(XINIM_ARCH_ARM64)
    vst1q_u8(static_cast<uint8_t*>(ptr), v.neon);
#endif
}

// XOR operation
inline vec128 xor_vec(vec128 a, vec128 b) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    result.xmm = _mm_xor_si128(a.xmm, b.xmm);
#elif defined(XINIM_ARCH_ARM64)
    result.neon = veorq_u8(a.neon, b.neon);
#endif
    return result;
}

// AND operation
inline vec128 and_vec(vec128 a, vec128 b) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    result.xmm = _mm_and_si128(a.xmm, b.xmm);
#elif defined(XINIM_ARCH_ARM64)
    result.neon = vandq_u8(a.neon, b.neon);
#endif
    return result;
}

// OR operation
inline vec128 or_vec(vec128 a, vec128 b) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    result.xmm = _mm_or_si128(a.xmm, b.xmm);
#elif defined(XINIM_ARCH_ARM64)
    result.neon = vorrq_u8(a.neon, b.neon);
#endif
    return result;
}

// Rotate left for 32-bit elements
inline vec128 rotl32(vec128 v, int count) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    if (count == 8 || count == 16 || count == 24) {
        // Use byte shuffle for multiples of 8
        result.xmm = _mm_shuffle_epi8(v.xmm, 
            _mm_set_epi8(12+count/8, 13+count/8, 14+count/8, 15+count/8,
                        8+count/8, 9+count/8, 10+count/8, 11+count/8,
                        4+count/8, 5+count/8, 6+count/8, 7+count/8,
                        0+count/8, 1+count/8, 2+count/8, 3+count/8));
    } else {
        // Generic rotate using shifts
        __m128i left = _mm_slli_epi32(v.xmm, count);
        __m128i right = _mm_srli_epi32(v.xmm, 32 - count);
        result.xmm = _mm_or_si128(left, right);
    }
#elif defined(XINIM_ARCH_ARM64)
    // ARM NEON doesn't have rotate, use shift+or
    uint32x4_t left = vshlq_n_u32(v.neon32, count);
    uint32x4_t right = vshrq_n_u32(v.neon32, 32 - count);
    result.neon32 = vorrq_u32(left, right);
#endif
    return result;
}

// Byte swap for endianness conversion
inline vec128 byte_swap32(vec128 v) noexcept {
    vec128 result;
#ifdef XINIM_ARCH_X86_64
    result.xmm = _mm_shuffle_epi8(v.xmm,
        _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3));
#elif defined(XINIM_ARCH_ARM64)
    result.neon32 = vrev32q_u8(vreinterpretq_u8_u32(v.neon32));
#endif
    return result;
}

// Population count
inline std::array<uint8_t, 16> popcount(vec128 v) noexcept {
    std::array<uint8_t, 16> result;
#ifdef XINIM_ARCH_X86_64
    // Use lookup table approach for SSE
    const __m128i lookup = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);
    __m128i low = _mm_and_si128(v.xmm, _mm_set1_epi8(0x0F));
    __m128i high = _mm_and_si128(_mm_srli_epi16(v.xmm, 4), _mm_set1_epi8(0x0F));
    __m128i low_count = _mm_shuffle_epi8(lookup, low);
    __m128i high_count = _mm_shuffle_epi8(lookup, high);
    __m128i total = _mm_add_epi8(low_count, high_count);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data()), total);
#elif defined(XINIM_ARCH_ARM64)
    uint8x16_t counts = vcntq_u8(v.neon);
    vst1q_u8(result.data(), counts);
#endif
    return result;
}

} // namespace xinim::hal::simd
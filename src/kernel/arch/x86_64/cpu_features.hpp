/**
 * @file cpu_features.hpp
 * @brief x86_64 CPU feature detection via CPUID.
 *
 * Freestanding -- no STL, no virtual dispatch.
 * Call cpu_detect_features() once at boot; query flags afterward.
 */
#pragma once

#include <cstdint>

namespace xinim::arch::x86_64 {

struct CpuFeatures {
    // Leaf 1 ECX
    bool sse3       : 1;
    bool pclmulqdq  : 1;
    bool ssse3      : 1;
    bool sse41      : 1;
    bool sse42      : 1;
    bool aesni      : 1;
    bool avx        : 1;
    bool rdrand     : 1;

    // Leaf 7, subleaf 0 EBX
    bool avx2       : 1;
    bool bmi1       : 1;
    bool bmi2       : 1;
    bool sha_ni     : 1;  // SHA1/SHA256 extensions

    // Leaf 7, subleaf 0 ECX
    bool vaes       : 1;  // Vectorized AES (AVX-512/AVX2)
    bool vpclmulqdq : 1;

    // Leaf 0x80000001 EDX
    bool nx_bit     : 1;
    bool long_mode  : 1;

    // Leaf 7, subleaf 0 EDX
    bool avx512f    : 1;
};

inline void cpuid(uint32_t leaf, uint32_t subleaf,
                  uint32_t& eax, uint32_t& ebx,
                  uint32_t& ecx, uint32_t& edx) noexcept {
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(leaf), "c"(subleaf));
}

inline CpuFeatures cpu_detect_features() noexcept {
    CpuFeatures f{};
    uint32_t eax, ebx, ecx, edx;

    // Leaf 1: basic features
    cpuid(1, 0, eax, ebx, ecx, edx);
    f.sse3       = (ecx >> 0) & 1;
    f.pclmulqdq  = (ecx >> 1) & 1;
    f.ssse3      = (ecx >> 9) & 1;
    f.sse41      = (ecx >> 19) & 1;
    f.sse42      = (ecx >> 20) & 1;
    f.aesni      = (ecx >> 25) & 1;
    f.avx        = (ecx >> 28) & 1;
    f.rdrand     = (ecx >> 30) & 1;

    // Leaf 7, subleaf 0: extended features
    cpuid(7, 0, eax, ebx, ecx, edx);
    f.bmi1       = (ebx >> 3) & 1;
    f.avx2       = (ebx >> 5) & 1;
    f.bmi2       = (ebx >> 8) & 1;
    f.sha_ni     = (ebx >> 29) & 1;
    f.vaes       = (ecx >> 9) & 1;
    f.vpclmulqdq = (ecx >> 10) & 1;
    f.avx512f    = (edx >> 16) & 1;

    // Leaf 0x80000001: extended processor info
    cpuid(0x80000001, 0, eax, ebx, ecx, edx);
    f.nx_bit     = (edx >> 20) & 1;
    f.long_mode  = (edx >> 29) & 1;

    return f;
}

// Global instance, set once at boot
inline CpuFeatures g_cpu_features{};

} // namespace xinim::arch::x86_64

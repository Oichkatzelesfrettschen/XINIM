/**
 * @file detect.hpp
 * @brief SIMD capability detection for all supported architectures
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#pragma once

#include <xinim/simd/core.hpp>
#include <cstdint>

namespace xinim::simd {

/**
 * @brief Architecture-specific capability detection
 */
namespace detail {

#if defined(__x86_64__) || defined(_M_X64)

/**
 * @brief x86-64 CPUID wrapper
 */
struct CpuidResult {
    std::uint32_t eax, ebx, ecx, edx;
};

/**
 * @brief Execute CPUID instruction
 * @param leaf CPUID leaf (EAX input)
 * @param subleaf CPUID subleaf (ECX input)
 * @return CPUID result registers
 */
inline CpuidResult cpuid(std::uint32_t leaf, std::uint32_t subleaf = 0) noexcept {
    CpuidResult result{};
    
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("cpuid"
                 : "=a" (result.eax), "=b" (result.ebx), "=c" (result.ecx), "=d" (result.edx)
                 : "a" (leaf), "c" (subleaf)
                 : "memory");
#elif defined(_MSC_VER)
    int regs[4];
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
    result.eax = static_cast<std::uint32_t>(regs[0]);
    result.ebx = static_cast<std::uint32_t>(regs[1]);
    result.ecx = static_cast<std::uint32_t>(regs[2]);
    result.edx = static_cast<std::uint32_t>(regs[3]);
#endif
    
    return result;
}

/**
 * @brief Detect x86/x86-64 SIMD capabilities
 * @return Capability flags
 */
inline std::uint64_t detect_x86_capabilities() noexcept {
    std::uint64_t caps = 0;
    
    // Check if CPUID is supported
    auto cpuid_info = cpuid(0);
    if (cpuid_info.eax == 0) {
        return caps; // No CPUID support
    }
    
    // Basic feature information (leaf 1)
    auto basic_features = cpuid(1);
    
    // Standard features in ECX
    if (basic_features.ecx & (1U << 0))  caps |= static_cast<std::uint64_t>(Capability::SSE3);
    if (basic_features.ecx & (1U << 9))  caps |= static_cast<std::uint64_t>(Capability::SSSE3);
    if (basic_features.ecx & (1U << 19)) caps |= static_cast<std::uint64_t>(Capability::SSE4_1);
    if (basic_features.ecx & (1U << 20)) caps |= static_cast<std::uint64_t>(Capability::SSE4_2);
    if (basic_features.ecx & (1U << 28)) caps |= static_cast<std::uint64_t>(Capability::AVX);
    if (basic_features.ecx & (1U << 12)) caps |= static_cast<std::uint64_t>(Capability::FMA3);
    
    // Standard features in EDX  
    if (basic_features.edx & (1U << 0))  caps |= static_cast<std::uint64_t>(Capability::X87_FPU);
    if (basic_features.edx & (1U << 23)) caps |= static_cast<std::uint64_t>(Capability::MMX);
    if (basic_features.edx & (1U << 25)) caps |= static_cast<std::uint64_t>(Capability::SSE);
    if (basic_features.edx & (1U << 26)) caps |= static_cast<std::uint64_t>(Capability::SSE2);
    
    // Extended features (leaf 7, subleaf 0)
    if (cpuid_info.eax >= 7) {
        auto extended_features = cpuid(7, 0);
        
        if (extended_features.ebx & (1U << 5))  caps |= static_cast<std::uint64_t>(Capability::AVX2);
        if (extended_features.ebx & (1U << 16)) caps |= static_cast<std::uint64_t>(Capability::AVX512F);
        if (extended_features.ebx & (1U << 17)) caps |= static_cast<std::uint64_t>(Capability::AVX512DQ);
        if (extended_features.ebx & (1U << 21)) caps |= static_cast<std::uint64_t>(Capability::AVX512CD);
        if (extended_features.ebx & (1U << 26)) caps |= static_cast<std::uint64_t>(Capability::AVX512PF);
        if (extended_features.ebx & (1U << 27)) caps |= static_cast<std::uint64_t>(Capability::AVX512ER);
        if (extended_features.ebx & (1U << 28)) caps |= static_cast<std::uint64_t>(Capability::AVX512CD);
        if (extended_features.ebx & (1U << 30)) caps |= static_cast<std::uint64_t>(Capability::AVX512BW);
        if (extended_features.ebx & (1U << 31)) caps |= static_cast<std::uint64_t>(Capability::AVX512VL);
        
        if (extended_features.ecx & (1U << 11)) caps |= static_cast<std::uint64_t>(Capability::AVX512VNNI);
    }
    
    // AMD extended features
    auto extended_info = cpuid(0x80000000);
    if (extended_info.eax >= 0x80000001) {
        auto amd_features = cpuid(0x80000001);
        
        if (amd_features.edx & (1U << 31)) caps |= static_cast<std::uint64_t>(Capability::AMD_3DNOW);
        if (amd_features.edx & (1U << 30)) caps |= static_cast<std::uint64_t>(Capability::AMD_3DNOWEXT);
        if (amd_features.ecx & (1U << 6))  caps |= static_cast<std::uint64_t>(Capability::SSE4A);
        if (amd_features.ecx & (1U << 16)) caps |= static_cast<std::uint64_t>(Capability::FMA4);
    }
    
    return caps;
}

#else
#error "XINIM only supports x86_64 architecture. Ensure __x86_64__ or _M_X64 is defined. Compile with -march=x86-64 or appropriate 64-bit target."
#endif

} // namespace detail

/**
 * @brief Global capability detection function
 * @return Detected SIMD capabilities
 */
inline std::uint64_t detect_capabilities() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    return detail::detect_x86_capabilities();
#else
    #error "XINIM only supports x86_64 architecture"
#endif
}

/**
 * @brief Get architecture name string
 * @return Architecture name
 */
constexpr const char* get_architecture_name() noexcept {
    return "x86-64";
}

/**
 * @brief Get capability name string
 * @param cap Capability to describe
 * @return Human-readable capability name
 */
constexpr const char* get_capability_name(Capability cap) noexcept {
    switch (cap) {
        case Capability::X87_FPU:       return "X87 FPU";
        case Capability::MMX:           return "MMX";
        case Capability::SSE:           return "SSE";
        case Capability::SSE2:          return "SSE2";
        case Capability::SSE3:          return "SSE3";
        case Capability::SSSE3:         return "SSSE3";
        case Capability::SSE4_1:        return "SSE4.1";
        case Capability::SSE4_2:        return "SSE4.2";
        case Capability::SSE4A:         return "SSE4a";
        case Capability::FMA3:          return "FMA3";
        case Capability::FMA4:          return "FMA4";
        case Capability::AVX:           return "AVX";
        case Capability::AVX2:          return "AVX2";
        case Capability::AVX512F:       return "AVX-512F";
        case Capability::AVX512VL:      return "AVX-512VL";
        case Capability::AVX512BW:      return "AVX-512BW";
        case Capability::AVX512DQ:      return "AVX-512DQ";
        case Capability::AVX512CD:      return "AVX-512CD";
        case Capability::AVX512ER:      return "AVX-512ER";
        case Capability::AVX512PF:      return "AVX-512PF";
        case Capability::AVX512VNNI:    return "AVX-512VNNI";
        case Capability::AMD_3DNOW:     return "3DNow!";
        case Capability::AMD_3DNOWEXT:  return "3DNow! Extended";
        default:                        return "Unknown";
    }
}

} // namespace xinim::simd

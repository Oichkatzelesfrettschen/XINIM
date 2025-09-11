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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

/**
 * @brief x86/x86-64 CPUID wrapper
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

#elif defined(__aarch64__) || defined(_M_ARM64)

/**
 * @brief Read ARM system register
 * @param reg Register name
 * @return Register value
 */
inline std::uint64_t read_system_register(const char* reg) noexcept {
    std::uint64_t value = 0;
#if defined(__GNUC__) || defined(__clang__)
    // Note: This requires runtime register name which isn't supported
    // For now, return 0 and rely on OS detection
    (void)reg;
#endif
    return value;
}

/**
 * @brief Detect ARM64 SIMD capabilities
 * @return Capability flags
 */
inline std::uint64_t detect_arm64_capabilities() noexcept {
    std::uint64_t caps = 0;
    
#if defined(__GNUC__) || defined(__clang__)
    // Read ID_AA64PFR0_EL1 (Processor Feature Register 0)
    std::uint64_t pfr0;
    asm volatile("mrs %0, id_aa64pfr0_el1" : "=r" (pfr0));
    
    // Check FP field (bits 19:16)
    std::uint64_t fp = (pfr0 >> 16) & 0xF;
    if (fp >= 0x0) caps |= static_cast<std::uint64_t>(Capability::VFP);
    if (fp >= 0x1) caps |= static_cast<std::uint64_t>(Capability::VFP4);
    
    // Check AdvSIMD field (bits 23:20)  
    std::uint64_t advsimd = (pfr0 >> 20) & 0xF;
    if (advsimd >= 0x0) caps |= static_cast<std::uint64_t>(Capability::NEON);
    if (advsimd >= 0x1) caps |= static_cast<std::uint64_t>(Capability::NEON_FP16);
    
    // Check SVE field (bits 35:32)
    std::uint64_t sve = (pfr0 >> 32) & 0xF;
    if (sve >= 0x1) caps |= static_cast<std::uint64_t>(Capability::SVE);
    
    // Read ID_AA64ISAR0_EL1 (Instruction Set Attribute Register 0)
    std::uint64_t isar0;
    asm volatile("mrs %0, id_aa64isar0_el1" : "=r" (isar0));
    
    // Check AES field (bits 7:4)
    std::uint64_t aes = (isar0 >> 4) & 0xF;
    if (aes >= 0x1) caps |= static_cast<std::uint64_t>(Capability::CRYPTO);
    
    // Check CRC32 field (bits 19:16)
    std::uint64_t crc32 = (isar0 >> 16) & 0xF;
    if (crc32 >= 0x1) caps |= static_cast<std::uint64_t>(Capability::CRC32);
    
    // Read ID_AA64ZFR0_EL1 (SVE Feature Register) if SVE is present
    if (caps & static_cast<std::uint64_t>(Capability::SVE)) {
        std::uint64_t zfr0;
        asm volatile("mrs %0, id_aa64zfr0_el1" : "=r" (zfr0));
        
        // Check SVE version
        std::uint64_t svever = zfr0 & 0xF;
        if (svever >= 0x1) caps |= static_cast<std::uint64_t>(Capability::SVE2);
    }
#endif
    
    return caps;
}

#elif defined(__riscv)

/**
 * @brief Detect RISC-V vector capabilities
 * @return Capability flags
 */
inline std::uint64_t detect_riscv_capabilities() noexcept {
    std::uint64_t caps = 0;
    
    // RISC-V vector detection would go here
    // This is future work as RISC-V vector extensions are still evolving
    
    return caps;
}

#endif

} // namespace detail

/**
 * @brief Global capability detection function
 * @return Detected SIMD capabilities
 */
inline std::uint64_t detect_capabilities() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    return detail::detect_x86_capabilities();
#elif defined(__aarch64__) || defined(_M_ARM64)
    return detail::detect_arm64_capabilities();
#elif defined(__riscv)
    return detail::detect_riscv_capabilities();
#else
    return 0; // No SIMD support detected
#endif
}

/**
 * @brief Get architecture name string
 * @return Architecture name
 */
constexpr const char* get_architecture_name() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86-64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
#elif defined(__arm__) || defined(_M_ARM)
    return "ARM32";
#elif defined(__riscv)
    return "RISC-V";
#else
    return "Unknown";
#endif
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
        case Capability::VFP:           return "VFP";
        case Capability::VFP3:          return "VFPv3";
        case Capability::VFP4:          return "VFPv4";
        case Capability::NEON:          return "NEON";
        case Capability::NEON_FP16:     return "NEON FP16";
        case Capability::CRYPTO:        return "ARM Crypto";
        case Capability::CRC32:         return "CRC32";
        case Capability::SVE:           return "SVE";
        case Capability::SVE2:          return "SVE2";
        case Capability::RV_V:          return "RISC-V Vector";
        case Capability::RV_ZVL128B:    return "RISC-V ZVL128B";
        case Capability::RV_ZVL256B:    return "RISC-V ZVL256B";
        case Capability::RV_ZVL512B:    return "RISC-V ZVL512B";
        default:                        return "Unknown";
    }
}

} // namespace xinim::simd

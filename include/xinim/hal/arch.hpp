// XINIM Hardware Abstraction Layer - Architecture Detection
#pragma once

#include <cstdint>
#include <concepts>
#include <bit>

namespace xinim::hal {

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define XINIM_ARCH_X86_64 1
    #define XINIM_ARCH_NAME "x86_64"
    inline constexpr bool is_x86_64 = true;
    inline constexpr bool is_arm64 = false;
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define XINIM_ARCH_ARM64 1
    #define XINIM_ARCH_NAME "arm64"
    inline constexpr bool is_x86_64 = false;
    inline constexpr bool is_arm64 = true;
#else
    #error "Unsupported architecture"
#endif

// Endianness detection
inline constexpr bool is_little_endian = std::endian::native == std::endian::little;
inline constexpr bool is_big_endian = std::endian::native == std::endian::big;

// Cache line size
#ifdef XINIM_ARCH_X86_64
    inline constexpr size_t cache_line_size = 64;
#elif defined(XINIM_ARCH_ARM64)
    inline constexpr size_t cache_line_size = 128; // Apple M1/M2
#endif

// Memory barriers
inline void memory_barrier() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("mfence" ::: "memory");
#elif defined(XINIM_ARCH_ARM64)
    asm volatile("dmb sy" ::: "memory");
#endif
}

inline void compiler_barrier() noexcept {
    asm volatile("" ::: "memory");
}

// CPU pause/yield for spinlocks
inline void cpu_pause() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("pause");
#elif defined(XINIM_ARCH_ARM64)
    asm volatile("yield");
#endif
}

// Prefetch hints
enum class prefetch_hint {
    read_low = 0,    // T0 - temporal, all cache levels
    read_medium = 1, // T1 - temporal, L2 and up
    read_high = 2,   // T2 - temporal, L3 and up  
    read_nta = 3,    // Non-temporal, minimize cache pollution
    write = 4        // Write prefetch
};

template<prefetch_hint Hint = prefetch_hint::read_low>
inline void prefetch(const void* addr) noexcept {
#ifdef XINIM_ARCH_X86_64
    if constexpr (Hint == prefetch_hint::read_low) {
        __builtin_prefetch(addr, 0, 3);
    } else if constexpr (Hint == prefetch_hint::read_medium) {
        __builtin_prefetch(addr, 0, 2);
    } else if constexpr (Hint == prefetch_hint::read_high) {
        __builtin_prefetch(addr, 0, 1);
    } else if constexpr (Hint == prefetch_hint::read_nta) {
        __builtin_prefetch(addr, 0, 0);
    } else if constexpr (Hint == prefetch_hint::write) {
        __builtin_prefetch(addr, 1, 3);
    }
#elif defined(XINIM_ARCH_ARM64)
    if constexpr (Hint == prefetch_hint::write) {
        asm volatile("prfm pstl1keep, [%0]" :: "r"(addr));
    } else {
        asm volatile("prfm pldl1keep, [%0]" :: "r"(addr));
    }
#endif
}

} // namespace xinim::hal
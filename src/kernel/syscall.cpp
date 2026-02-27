/**
 * @file syscall.cpp
 * @brief x86-64 system call configuration for the XINIM kernel.
 */

#include "../include/defs.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"

#ifdef __x86_64__

inline constexpr u32_t MSR_EFER{0xC000'0080U};
inline constexpr u32_t MSR_STAR{0xC000'0081U};
inline constexpr u32_t MSR_LSTAR{0xC000'0082U};
inline constexpr u32_t MSR_FMASK{0xC000'0084U};

extern "C" void syscall_entry() noexcept;

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = static_cast<uint32_t>(value);
    uint32_t high = static_cast<uint32_t>(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

void init_syscall_msrs() noexcept {
    // EFER: Enable SCE (bit 0)
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_EFER));
    wrmsr(MSR_EFER, static_cast<uint64_t>(low | 1) | (static_cast<uint64_t>(high) << 32));

    // STAR: Kernel CS = 0x08, User CS = 0x1B (Entry 3 in GDT)
    // STAR layout: [63:48] User CS+16, [47:32] Kernel CS, [31:0] ignored
    uint64_t star = (static_cast<uint64_t>(0x1B) << 48) | (static_cast<uint64_t>(0x08) << 32);
    wrmsr(MSR_STAR, star);

    // LSTAR: Entry point address
    wrmsr(MSR_LSTAR, reinterpret_cast<uint64_t>(syscall_entry));

    // FMASK: Mask all flags for now
    wrmsr(MSR_FMASK, 0xFFFFFFFF);
}

#endif 

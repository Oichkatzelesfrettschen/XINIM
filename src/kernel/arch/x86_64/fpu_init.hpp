/**
 * @file fpu_init.hpp
 * @brief Initialize CR0/CR4 bits required for FXSAVE/FXRSTOR and SSE.
 *
 * Must be called early in boot, before any context switch or SSE use.
 * Without these bits set, FXSAVE/FXRSTOR will trigger #UD.
 *
 * CR0 bits:
 *   EM (bit 2) = 0  -- no FPU emulation; real FPU present
 *   MP (bit 1) = 1  -- monitor co-processor
 *   NE (bit 5) = 1  -- native FPU error reporting (not IRQ 13)
 *
 * CR4 bits:
 *   OSFXSR    (bit 9)  = 1  -- OS supports FXSAVE/FXRSTOR for SSE
 *   OSXMMEXCPT (bit 10) = 1 -- OS handles unmasked SIMD FP exceptions (#XM)
 */
#pragma once

#include <cstdint>

namespace xinim::arch::x86_64 {

inline void fpu_init() noexcept {
    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);              // Clear EM (bit 2)
    cr0 |=  (1ULL << 1) | (1ULL << 5); // Set MP (bit 1) + NE (bit 5)
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); // Set OSFXSR + OSXMMEXCPT
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
}

} // namespace xinim::arch::x86_64

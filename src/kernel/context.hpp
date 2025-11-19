/**
 * @file context.hpp
 * @brief CPU context structures for process/thread switching
 *
 * Defines complete CPU state for x86_64 and ARM64 architectures.
 * Used for context switching during preemptive scheduling.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_CONTEXT_HPP
#define XINIM_KERNEL_CONTEXT_HPP

#include <cstdint>

namespace xinim::kernel {

#ifdef XINIM_ARCH_X86_64

/**
 * @brief Complete CPU context for x86_64
 *
 * Stores ALL registers needed for full context switch.
 * This structure is saved/restored on every timer interrupt.
 *
 * Layout matches the order registers are pushed/popped in assembly.
 */
struct CpuContext_x86_64 {
    // General Purpose Registers (in push/pop order)
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    // Segment selectors
    uint64_t gs;
    uint64_t fs;
    uint64_t es;
    uint64_t ds;

    // Interrupt frame (pushed by CPU during interrupt)
    uint64_t rip;     // Instruction pointer
    uint64_t cs;      // Code segment
    uint64_t rflags;  // CPU flags
    uint64_t rsp;     // Stack pointer
    uint64_t ss;      // Stack segment

    // Control registers
    uint64_t cr3;     // Page directory base (for memory isolation)

    /**
     * @brief Initialize context for a new process
     * @param entry_point Function to execute
     * @param stack_top Top of stack (stack grows downward)
     * @param ring Privilege level (0=kernel, 3=user)
     */
    void initialize(uint64_t entry_point, uint64_t stack_top, int ring = 0) {
        // Zero all registers
        r15 = r14 = r13 = r12 = r11 = r10 = r9 = r8 = 0;
        rbp = rdi = rsi = rdx = rcx = rbx = rax = 0;

        // Set up instruction pointer and stack
        rip = entry_point;
        rsp = stack_top;

        // Set up segment selectors
        if (ring == 0) {
            // Kernel mode (Ring 0)
            cs = 0x08;   // Kernel code segment
            ss = 0x10;   // Kernel data/stack segment
            ds = 0x10;
            es = 0x10;
            fs = 0x10;
            gs = 0x10;
        } else {
            // User mode (Ring 3)
            cs = 0x1B;   // User code segment (0x18 | RPL=3)
            ss = 0x23;   // User data/stack segment (0x20 | RPL=3)
            ds = 0x23;
            es = 0x23;
            fs = 0x23;
            gs = 0x23;
        }

        // Set up RFLAGS
        rflags = 0x202;  // IF=1 (interrupts enabled), bit 1 always set

        // CR3 will be set later when page tables are created
        cr3 = 0;
    }
} __attribute__((packed));

using CpuContext = CpuContext_x86_64;

// Size: 26 * 8 = 208 bytes

#elif defined(XINIM_ARCH_ARM64)

/**
 * @brief Complete CPU context for ARM64
 *
 * Stores ALL registers needed for full context switch.
 */
struct CpuContext_arm64 {
    // General purpose registers
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8, x9, x10, x11, x12, x13, x14, x15;
    uint64_t x16, x17, x18, x19, x20, x21, x22, x23;
    uint64_t x24, x25, x26, x27, x28;

    // Frame pointer, link register, stack pointer
    uint64_t x29;  // FP
    uint64_t x30;  // LR (link register)
    uint64_t sp;   // Stack pointer

    // Program counter and processor state
    uint64_t pc;     // Program counter
    uint64_t pstate; // Processor state

    // Exception level and translation table base
    uint64_t ttbr0;  // Translation table base (user)
    uint64_t ttbr1;  // Translation table base (kernel)

    void initialize(uint64_t entry_point, uint64_t stack_top, int el = 0) {
        // Zero all registers
        x0 = x1 = x2 = x3 = x4 = x5 = x6 = x7 = 0;
        x8 = x9 = x10 = x11 = x12 = x13 = x14 = x15 = 0;
        x16 = x17 = x18 = x19 = x20 = x21 = x22 = x23 = 0;
        x24 = x25 = x26 = x27 = x28 = x29 = x30 = 0;

        // Set up PC and SP
        pc = entry_point;
        sp = stack_top;

        // Set up processor state (EL0 or EL1)
        pstate = (el == 0) ? 0x0 : 0x5;  // EL0 or EL1

        // TTBRs will be set later
        ttbr0 = ttbr1 = 0;
    }
} __attribute__((packed));

using CpuContext = CpuContext_arm64;

#else
#error "Unsupported architecture"
#endif

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_CONTEXT_HPP */

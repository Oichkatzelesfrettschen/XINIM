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

#include <cstddef>
#include <cstdint>
#include <xinim/abi/x86_64_context_layout.h>

namespace xinim::kernel {

#ifdef XINIM_ARCH_X86_64

    /**
     * @brief Complete CPU context for x86_64
     *
     * Stores the registers consumed by the non-returning context restore entries.
     * The x86_64 timer path does not yet capture its interrupt frame into this
     * structure, so this type must not be presented as runtime preemption evidence.
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
        uint64_t rip;    // Instruction pointer
        uint64_t cs;     // Code segment
        uint64_t rflags; // CPU flags
        uint64_t rsp;    // Stack pointer
        uint64_t ss;     // Stack segment

        // Control registers
        uint64_t cr3; // Page directory base (for memory isolation)

        // x87, MMX, and SSE state in the 512-byte FXSAVE region.
        // Must be 16-byte aligned for FXSAVE/FXRSTOR instructions.
        alignas(XINIM_X86_64_FXSAVE_ALIGNMENT) uint8_t fxsave_area[XINIM_X86_64_FXSAVE_SIZE];

        /**
         * @brief Initialize context for a new process
         * @param entry_point Function to execute
         * @param stack_top Top of stack (stack grows downward)
         * @param ring Privilege level (0=kernel, 3=user)
         */
        constexpr void initialize(uint64_t entry_point, uint64_t stack_top, int ring = 0) {
            // Zero all registers
            r15 = r14 = r13 = r12 = r11 = r10 = r9 = r8 = 0;
            rbp = rdi = rsi = rdx = rcx = rbx = rax = 0;

            // Set up instruction pointer and stack
            rip = entry_point;
            rsp = stack_top;

            // Set up segment selectors
            if (ring == 0) {
                // Kernel mode (Ring 0)
                cs = XINIM_X86_KERNEL_CS_SELECTOR;
                ss = XINIM_X86_KERNEL_DS_SELECTOR;
                ds = XINIM_X86_KERNEL_DS_SELECTOR;
                es = XINIM_X86_KERNEL_DS_SELECTOR;
                fs = XINIM_X86_KERNEL_DS_SELECTOR;
                gs = XINIM_X86_KERNEL_DS_SELECTOR;
            } else {
                // User mode (Ring 3)
                cs = XINIM_X86_USER_CS_SELECTOR;
                ss = XINIM_X86_USER_DS_SELECTOR;
                ds = XINIM_X86_USER_DS_SELECTOR;
                es = XINIM_X86_USER_DS_SELECTOR;
                fs = XINIM_X86_USER_DS_SELECTOR;
                gs = XINIM_X86_USER_DS_SELECTOR;
            }

            // Set up RFLAGS
            rflags = XINIM_X86_64_CONTEXT_INITIAL_RFLAGS;

            // CR3 will be set later when page tables are created
            cr3 = 0;

            // Initialize an architectural reset-state FXSAVE image. FCW 0x037F
            // masks x87 exceptions and selects round-to-nearest. A zero FTW marks
            // every x87 register empty. MXCSR 0x1F80 masks SSE exceptions and
            // selects round-to-nearest.
            for (uint8_t &state_byte : fxsave_area) {
                state_byte = 0;
            }
            fxsave_area[XINIM_X86_64_FXSAVE_FCW_OFFSET] =
                static_cast<uint8_t>(XINIM_X86_64_FXSAVE_RESET_FCW);
            fxsave_area[XINIM_X86_64_FXSAVE_FCW_OFFSET + 1] = static_cast<uint8_t>(
                XINIM_X86_64_FXSAVE_RESET_FCW >> XINIM_X86_64_FXSAVE_BITS_PER_BYTE);
            fxsave_area[XINIM_X86_64_FXSAVE_MXCSR_OFFSET] =
                static_cast<uint8_t>(XINIM_X86_64_FXSAVE_RESET_MXCSR);
            fxsave_area[XINIM_X86_64_FXSAVE_MXCSR_OFFSET + 1] = static_cast<uint8_t>(
                XINIM_X86_64_FXSAVE_RESET_MXCSR >> XINIM_X86_64_FXSAVE_BITS_PER_BYTE);
        }
    };

    static_assert(offsetof(CpuContext_x86_64, r15) == XINIM_X86_64_CONTEXT_R15);
    static_assert(offsetof(CpuContext_x86_64, r14) == XINIM_X86_64_CONTEXT_R14);
    static_assert(offsetof(CpuContext_x86_64, r13) == XINIM_X86_64_CONTEXT_R13);
    static_assert(offsetof(CpuContext_x86_64, r12) == XINIM_X86_64_CONTEXT_R12);
    static_assert(offsetof(CpuContext_x86_64, r11) == XINIM_X86_64_CONTEXT_R11);
    static_assert(offsetof(CpuContext_x86_64, r10) == XINIM_X86_64_CONTEXT_R10);
    static_assert(offsetof(CpuContext_x86_64, r9) == XINIM_X86_64_CONTEXT_R9);
    static_assert(offsetof(CpuContext_x86_64, r8) == XINIM_X86_64_CONTEXT_R8);
    static_assert(offsetof(CpuContext_x86_64, rbp) == XINIM_X86_64_CONTEXT_RBP);
    static_assert(offsetof(CpuContext_x86_64, rdi) == XINIM_X86_64_CONTEXT_RDI);
    static_assert(offsetof(CpuContext_x86_64, rsi) == XINIM_X86_64_CONTEXT_RSI);
    static_assert(offsetof(CpuContext_x86_64, rdx) == XINIM_X86_64_CONTEXT_RDX);
    static_assert(offsetof(CpuContext_x86_64, rcx) == XINIM_X86_64_CONTEXT_RCX);
    static_assert(offsetof(CpuContext_x86_64, rbx) == XINIM_X86_64_CONTEXT_RBX);
    static_assert(offsetof(CpuContext_x86_64, rax) == XINIM_X86_64_CONTEXT_RAX);
    static_assert(offsetof(CpuContext_x86_64, gs) == XINIM_X86_64_CONTEXT_GS);
    static_assert(offsetof(CpuContext_x86_64, fs) == XINIM_X86_64_CONTEXT_FS);
    static_assert(offsetof(CpuContext_x86_64, es) == XINIM_X86_64_CONTEXT_ES);
    static_assert(offsetof(CpuContext_x86_64, ds) == XINIM_X86_64_CONTEXT_DS);
    static_assert(offsetof(CpuContext_x86_64, rip) == XINIM_X86_64_CONTEXT_RIP);
    static_assert(offsetof(CpuContext_x86_64, cs) == XINIM_X86_64_CONTEXT_CS);
    static_assert(offsetof(CpuContext_x86_64, rflags) == XINIM_X86_64_CONTEXT_RFLAGS);
    static_assert(offsetof(CpuContext_x86_64, rsp) == XINIM_X86_64_CONTEXT_RSP);
    static_assert(offsetof(CpuContext_x86_64, ss) == XINIM_X86_64_CONTEXT_SS);
    static_assert(offsetof(CpuContext_x86_64, cr3) == XINIM_X86_64_CONTEXT_CR3);
    static_assert(offsetof(CpuContext_x86_64, fxsave_area) == XINIM_X86_64_CONTEXT_FXSAVE);
    static_assert(sizeof(CpuContext_x86_64) == XINIM_X86_64_CONTEXT_SIZE);

    using CpuContext = CpuContext_x86_64;

    // Size: 26*8 + 512 = 720 bytes

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
        uint64_t x29; // FP
        uint64_t x30; // LR (link register)
        uint64_t sp;  // Stack pointer

        // Program counter and processor state
        uint64_t pc;     // Program counter
        uint64_t pstate; // Processor state

        // Exception level and translation table base
        uint64_t ttbr0; // Translation table base (user)
        uint64_t ttbr1; // Translation table base (kernel)

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
            pstate = (el == 0) ? 0x0 : 0x5; // EL0 or EL1

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

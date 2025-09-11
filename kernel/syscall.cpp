/**
 * @file syscall.cpp
 * @brief x86-64 and ARM64 system call entry and configuration for the XINIM kernel.
 *
 * This module defines the low-level entry points for system calls from
 * user mode to the kernel on x86-64 and ARM64 architectures. It configures the
 * necessary Model Specific Registers (MSRs) for x86-64 and exception vectors
 * for ARM64, providing fast and efficient transition mechanisms.
 *
 * The code uses architecture-specific assembly within C++ functions, declared
 * with modern `noexcept NAKED` specifiers for clarity and type safety. This
 * approach ensures that the low-level hardware interface is wrapped in a
 * clean and maintainable C++23 API.
 *
 * @ingroup kernel_arch
 */

#include "../include/defs.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"

#ifdef __x86_64__

/// @brief Model Specific Register identifiers used for system call setup.
inline constexpr u32_t MSR_EFER{0xC000'0080U};
inline constexpr u32_t MSR_STAR{0xC000'0081U};
inline constexpr u32_t MSR_LSTAR{0xC000'0082U};
inline constexpr u32_t MSR_FMASK{0xC000'0084U};
inline constexpr u32_t KERNEL_CS_SELECTOR{(0x18U << 16) | 0x18U};

/// Forward declaration of the system call entry point symbol.
void syscall_entry() noexcept;

/**
 * @brief Configure system call Model Specific Registers (MSRs).
 *
 * This function is called during kernel initialization to set up the MSRs
 * required for the `syscall` and `sysret` instructions on x86-64.
 * - EFER: Enables the syscall/sysret feature.
 * - STAR: Sets the kernel and user code segment selectors.
 * - LSTAR: Points to the system call entry point (`syscall_entry`).
 * - FMASK: Specifies the EFLAGS bits that will be masked on entry.
 */
inline void init_syscall_msrs() noexcept {
    asm volatile("mov $%c0, %%ecx\n\t"
                 "rdmsr\n\t"
                 "or $1, %%eax\n\t"
                 "wrmsr\n\t"
                 "mov $%c1, %%ecx\n\t"
                 "mov $%c2, %%eax\n\t"
                 "xor %%edx, %%edx\n\t"
                 "wrmsr\n\t"
                 "mov $%c3, %%ecx\n\t"
                 "mov $%c4, %%rax\n\t"
                 "wrmsr\n\t"
                 "mov $%c5, %%ecx\n\t"
                 "xor %%eax, %%eax\n\t"
                 "xor %%edx, %%edx\n\t"
                 "wrmsr"
                 :
                 : "i"(MSR_EFER), "i"(MSR_STAR), "i"(KERNEL_CS_SELECTOR), "i"(MSR_LSTAR),
                   "i"(syscall_entry), "i"(MSR_FMASK)
                 : "eax", "ecx", "edx", "memory");
}

/// @brief Entry point for user mode syscalls.
void syscall_entry() noexcept NAKED;

/**
 * @brief Assembly stub that dispatches a system call to the kernel handler.
 *
 * This function is the target of the LSTAR MSR. When a user process executes
 * the `syscall` instruction, control transfers here. It saves the user
 * context, sets up the arguments for the higher-level C++ handler (`_sys_call`),
 * and then resumes the process via `restart`.
 */
void syscall_entry() noexcept {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "call save\n\t"
                     "mov rax, rdi\n\t"
                     "mov rdi, rdx\n\t"
                     "mov esi, _cur_proc[rip]\n\t"
                     "mov rdx, rax\n\t"
                     "mov rcx, rsi\n\t"
                     "call _sys_call\n\t"
                     "jmp restart\n\t"
                     ".att_syntax prefix");
}

#elif defined(__aarch64__)

/**
 * @brief Configure ARM64 system call exception vectors.
 *
 * This function is called during kernel initialization to set up the
 * exception vectors for system calls on ARM64. The SVC (Supervisor Call)
 * instruction is used for system calls from user mode.
 */
inline void init_syscall_vectors() noexcept {
    // ARM64 uses exception vectors for system calls
    // The actual implementation would set up the VBAR_EL1 register
    // pointing to the exception vector table
    asm volatile(
        "adr x0, exception_vectors\n\t"
        "msr VBAR_EL1, x0\n\t"
        "isb"
        ::: "x0", "memory"
    );
}

/// @brief Entry point for ARM64 SVC (system call) exception.
void svc_entry() noexcept NAKED;

/**
 * @brief ARM64 system call handler entry point.
 *
 * This function handles the SVC exception on ARM64. When a user process
 * executes the `svc` instruction, control transfers here via the exception
 * vector. It saves the user context and dispatches to the C++ handler.
 */
void svc_entry() noexcept {
    __asm__ volatile(
        // Save context
        "stp x29, x30, [sp, #-16]!\n\t"
        "stp x27, x28, [sp, #-16]!\n\t"
        "stp x25, x26, [sp, #-16]!\n\t"
        "stp x23, x24, [sp, #-16]!\n\t"
        "stp x21, x22, [sp, #-16]!\n\t"
        "stp x19, x20, [sp, #-16]!\n\t"
        "stp x17, x18, [sp, #-16]!\n\t"
        "stp x15, x16, [sp, #-16]!\n\t"
        "stp x13, x14, [sp, #-16]!\n\t"
        "stp x11, x12, [sp, #-16]!\n\t"
        "stp x9, x10, [sp, #-16]!\n\t"
        "stp x7, x8, [sp, #-16]!\n\t"
        "stp x5, x6, [sp, #-16]!\n\t"
        "stp x3, x4, [sp, #-16]!\n\t"
        "stp x1, x2, [sp, #-16]!\n\t"
        "str x0, [sp, #-8]!\n\t"
        
        // Call C++ handler with syscall number in x0
        "bl _sys_call\n\t"
        
        // Restore context
        "ldr x0, [sp], #8\n\t"
        "ldp x1, x2, [sp], #16\n\t"
        "ldp x3, x4, [sp], #16\n\t"
        "ldp x5, x6, [sp], #16\n\t"
        "ldp x7, x8, [sp], #16\n\t"
        "ldp x9, x10, [sp], #16\n\t"
        "ldp x11, x12, [sp], #16\n\t"
        "ldp x13, x14, [sp], #16\n\t"
        "ldp x15, x16, [sp], #16\n\t"
        "ldp x17, x18, [sp], #16\n\t"
        "ldp x19, x20, [sp], #16\n\t"
        "ldp x21, x22, [sp], #16\n\t"
        "ldp x23, x24, [sp], #16\n\t"
        "ldp x25, x26, [sp], #16\n\t"
        "ldp x27, x28, [sp], #16\n\t"
        "ldp x29, x30, [sp], #16\n\t"
        
        // Return from exception
        "eret"
    );
}

#endif // Architecture selection
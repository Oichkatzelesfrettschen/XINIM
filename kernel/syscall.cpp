/**
 * @file syscall.cpp
 * @brief x86-64 system call entry and configuration for the XINIM kernel.
 *
 * This module defines the low-level entry points for system calls from
 * user mode to the kernel on the x86-64 architecture. It configures the
 * necessary Model Specific Registers (MSRs) to enable the `syscall` and
 * `sysret` instructions, providing a fast and efficient transition mechanism.
 *
 * The code uses hand-crafted assembly within C++ functions, which are
 * declared with modern `noexcept NAKED` specifiers for clarity and type safety.
 * This approach ensures that the low-level hardware interface is wrapped in a
 * clean and maintainable C++23 API.
 *
 * @ingroup kernel_arch
 */

#include "../include/defs.h"
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

#endif
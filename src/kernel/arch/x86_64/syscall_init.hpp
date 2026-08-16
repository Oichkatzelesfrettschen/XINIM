/**
 * @file syscall_init.hpp
 * @brief Syscall initialization for x86_64
 *
 * Sets up Model Specific Registers (MSRs) to enable fast syscall mechanism.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 */

#ifndef XINIM_KERNEL_ARCH_X86_64_SYSCALL_INIT_HPP
#define XINIM_KERNEL_ARCH_X86_64_SYSCALL_INIT_HPP

#include <cstdint>

namespace xinim::kernel {

    /**
     * @brief Initialize the syscall mechanism
     *
     * Configures MSRs to enable fast system calls:
     * - IA32_EFER: Enable SCE (System Call Extensions)
     * - IA32_STAR: Set kernel/user CS selectors
     * - IA32_LSTAR: Set syscall handler address
     * - IA32_FMASK: Set RFLAGS mask (disable interrupts)
     *
     * Must be called during kernel initialization before enabling Ring 3.
     */
    void initialize_syscall();
    void set_syscall_kernel_stack(uint64_t stack_top) noexcept;

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_ARCH_X86_64_SYSCALL_INIT_HPP */

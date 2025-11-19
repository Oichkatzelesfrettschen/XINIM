/**
 * @file tss.hpp
 * @brief Task State Segment (TSS) for x86_64
 *
 * Provides kernel stack management for privilege level transitions.
 * When a Ring 3 process is interrupted, the CPU loads RSP0 from the TSS.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_ARCH_X86_64_TSS_HPP
#define XINIM_KERNEL_ARCH_X86_64_TSS_HPP

#include <cstdint>

namespace xinim::kernel {

// ============================================================================
// TSS Management Functions
// ============================================================================

/**
 * @brief Initialize Task State Segment
 *
 * Creates a TSS and adds its descriptor to the GDT.
 * The TSS contains the kernel stack pointer (RSP0) that
 * the CPU uses when transitioning from Ring 3 to Ring 0.
 *
 * Must be called AFTER initialize_gdt().
 */
void initialize_tss();

/**
 * @brief Set kernel stack pointer in TSS
 *
 * Updates TSS.rsp0 to point to the given kernel stack.
 * This MUST be called before switching to any Ring 3 process.
 *
 * @param kernel_rsp Kernel stack pointer (top of kernel stack)
 *
 * When an interrupt occurs in Ring 3, the CPU will:
 * 1. Read TSS.rsp0
 * 2. Switch to that stack
 * 3. Push interrupt frame onto kernel stack
 * 4. Call interrupt handler in Ring 0
 */
void set_kernel_stack(uint64_t kernel_rsp);

/**
 * @brief Get current kernel stack pointer from TSS
 *
 * @return Current TSS.rsp0 value
 */
uint64_t get_kernel_stack();

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_ARCH_X86_64_TSS_HPP */

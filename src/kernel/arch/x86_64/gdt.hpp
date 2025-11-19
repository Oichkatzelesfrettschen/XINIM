/**
 * @file gdt.hpp
 * @brief Global Descriptor Table (GDT) for x86_64
 *
 * Provides privilege level separation (Ring 0 vs Ring 3) through
 * segment descriptors.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_ARCH_X86_64_GDT_HPP
#define XINIM_KERNEL_ARCH_X86_64_GDT_HPP

#include <cstdint>

namespace xinim::kernel {

// ============================================================================
// Segment Selector Constants
// ============================================================================

/**
 * @brief Kernel code segment selector (GDT entry 1, Ring 0)
 *
 * Binary: 0b00001000 = 0x08
 * Bits 0-1: RPL = 00 (Ring 0)
 * Bit 2: TI = 0 (GDT)
 * Bits 3-15: Index = 1
 */
constexpr uint16_t KERNEL_CS = 0x08;

/**
 * @brief Kernel data segment selector (GDT entry 2, Ring 0)
 *
 * Binary: 0b00010000 = 0x10
 * Bits 0-1: RPL = 00 (Ring 0)
 * Bit 2: TI = 0 (GDT)
 * Bits 3-15: Index = 2
 */
constexpr uint16_t KERNEL_DS = 0x10;

/**
 * @brief User code segment selector (GDT entry 3, Ring 3)
 *
 * Binary: 0b00011011 = 0x1B
 * Bits 0-1: RPL = 11 (Ring 3)
 * Bit 2: TI = 0 (GDT)
 * Bits 3-15: Index = 3
 */
constexpr uint16_t USER_CS = 0x1B;

/**
 * @brief User data segment selector (GDT entry 4, Ring 3)
 *
 * Binary: 0b00100011 = 0x23
 * Bits 0-1: RPL = 11 (Ring 3)
 * Bit 2: TI = 0 (GDT)
 * Bits 3-15: Index = 4
 */
constexpr uint16_t USER_DS = 0x23;

/**
 * @brief TSS selector (GDT entry 5, Ring 0)
 *
 * Binary: 0b00101000 = 0x28
 * Bits 0-1: RPL = 00 (Ring 0)
 * Bit 2: TI = 0 (GDT)
 * Bits 3-15: Index = 5
 */
constexpr uint16_t TSS_SEL = 0x28;

// ============================================================================
// GDT Management Functions
// ============================================================================

/**
 * @brief Initialize Global Descriptor Table
 *
 * Creates a 5-entry GDT with:
 * - Entry 0: Null descriptor (required)
 * - Entry 1: Kernel code (Ring 0, executable)
 * - Entry 2: Kernel data (Ring 0, writable)
 * - Entry 3: User code (Ring 3, executable)
 * - Entry 4: User data (Ring 3, writable)
 *
 * Note: TSS descriptor will be added by initialize_tss()
 *
 * Must be called during kernel initialization before enabling
 * Ring 3 processes.
 */
void initialize_gdt();

/**
 * @brief Get kernel code segment selector
 *
 * @return 0x08 (GDT entry 1, Ring 0)
 */
constexpr uint16_t get_kernel_cs() { return KERNEL_CS; }

/**
 * @brief Get kernel data segment selector
 *
 * @return 0x10 (GDT entry 2, Ring 0)
 */
constexpr uint16_t get_kernel_ds() { return KERNEL_DS; }

/**
 * @brief Get user code segment selector
 *
 * @return 0x1B (GDT entry 3, Ring 3)
 */
constexpr uint16_t get_user_cs() { return USER_CS; }

/**
 * @brief Get user data segment selector
 *
 * @return 0x23 (GDT entry 4, Ring 3)
 */
constexpr uint16_t get_user_ds() { return USER_DS; }

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_ARCH_X86_64_GDT_HPP */

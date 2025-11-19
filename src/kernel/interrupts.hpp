/**
 * @file interrupts.hpp
 * @brief Interrupt handler declarations
 *
 * Declares assembly interrupt handlers and related C++ functions.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_INTERRUPTS_HPP
#define XINIM_KERNEL_INTERRUPTS_HPP

#include <cstdint>

// ============================================================================
// Assembly Interrupt Handlers
// ============================================================================

/**
 * @brief Timer interrupt handler (IRQ 0, Vector 32)
 *
 * Defined in arch/x86_64/interrupts.S
 * This is the low-level assembly entry point for timer interrupts.
 */
extern "C" void timer_interrupt_handler();

/**
 * @brief Spurious interrupt handler (Vector 255)
 *
 * Defined in arch/x86_64/interrupts.S
 * Handles spurious APIC interrupts.
 */
extern "C" void spurious_interrupt_handler();

/**
 * @brief Generic interrupt handler (for unhandled interrupts)
 *
 * Defined in arch/x86_64/interrupts.S
 * Used for debugging unhandled interrupts.
 */
extern "C" void generic_interrupt_handler();

// ============================================================================
// Interrupt Initialization
// ============================================================================

/**
 * @brief Initialize Interrupt Descriptor Table (IDT)
 *
 * Sets up the IDT with all interrupt handlers.
 * Must be called during kernel initialization before enabling interrupts.
 */
void initialize_idt();

#endif /* XINIM_KERNEL_INTERRUPTS_HPP */

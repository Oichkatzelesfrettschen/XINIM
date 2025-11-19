/**
 * @file interrupts.cpp
 * @brief Interrupt setup and registration
 *
 * Initializes the IDT and registers interrupt handlers.
 * Updated for Week 8 Phase 2: Preemptive scheduling.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include <stdint.h>
#include "arch/x86_64/idt.hpp"
#include "interrupts.hpp"  // Week 8: New interrupt declarations
#include "early/serial_16550.hpp"
#include "../hal/x86_64/hal/apic.hpp"

// Week 8: Use new timer interrupt handler (defined in arch/x86_64/interrupts.S)
// This handler integrates with the scheduler for preemptive multitasking

namespace {
    xinim::early::Serial16550* g_serial = nullptr;
    xinim::hal::x86_64::Lapic* g_lapic  = nullptr;
}

/**
 * @brief Initialize interrupts
 *
 * Week 8 Phase 2: Register timer_interrupt_handler for preemptive scheduling.
 *
 * @param serial Early serial console for logging
 * @param lapic Local APIC for interrupt control
 */
void interrupts_init(xinim::early::Serial16550& serial, xinim::hal::x86_64::Lapic& lapic) {
    using namespace xinim::arch::x86_64::idt;

    g_serial = &serial;
    g_lapic  = &lapic;

    // Initialize IDT
    init();

    // Week 8 Phase 2: Register timer interrupt handler (vector 32 = IRQ 0)
    // This handler saves context, calls scheduler, and restores next process
    set_gate(32, timer_interrupt_handler, 0x8E, 0);

    serial.write("[IDT] Timer interrupt handler registered (vector 32)\n");
}


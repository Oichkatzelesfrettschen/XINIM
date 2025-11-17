/**
 * @file timer.cpp
 * @brief Timer interrupt handling and scheduler integration
 *
 * This module provides the C++ bridge between the assembly timer
 * interrupt handler and the scheduler.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "timer.hpp"
#include "scheduler.hpp"
#include "pcb.hpp"  // For ProcessState
#include "context.hpp"
#include "early/serial_16550.hpp"
#include "../hal/x86_64/hal/apic.hpp"  // For LAPIC EOI
#include <cstring>
#include <cstdio>  // For snprintf

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

static uint64_t g_timer_ticks = 0;
static xinim::hal::x86_64::Lapic* g_lapic = nullptr;

/**
 * @brief Send End of Interrupt (EOI) to APIC
 *
 * This must be called after handling any APIC interrupt
 * to allow the next interrupt to be delivered.
 *
 * Week 8: Uses LAPIC eoi() method instead of direct memory access
 * to avoid physical address mapping issues.
 */
static inline void send_apic_eoi() {
    if (g_lapic) {
        g_lapic->eoi();
    }
}

/**
 * @brief Get timer tick count
 */
uint64_t get_timer_ticks() {
    return g_timer_ticks;
}

} // namespace xinim::kernel

/**
 * @brief C++ timer interrupt handler
 *
 * Called from assembly interrupt handler with pointer to saved context.
 * This function is extern "C" so it can be called from assembly.
 *
 * @param context Pointer to saved CPU context on stack
 *
 * Important: The context pointer points to the current process's state
 * saved on the interrupt stack. The scheduler will perform a context
 * switch, which may change what RSP points to.
 */
extern "C" void timer_interrupt_c_handler(xinim::kernel::CpuContext* context) {
    using namespace xinim::kernel;

    // Increment tick counter
    g_timer_ticks++;

    // Get current process (before schedule() potentially switches)
    ProcessControlBlock* current = get_current_process();

    // If there's a current process, save its context from the interrupt stack
    if (current && current->state == ProcessState::RUNNING) {
        // Copy context from interrupt stack into PCB
        // This preserves the exact state when the interrupt occurred
        memcpy(&current->context, context, sizeof(CpuContext));
    }

    // Send EOI to APIC before scheduling
    // This allows nested interrupts if needed
    send_apic_eoi();

    // Call scheduler to pick next process
    // The scheduler will perform context_switch() if necessary
    // NOTE: After schedule() returns, we might be running in a different process!
    schedule();

    // If we're still here, we're in the "next" process now
    // The assembly stub will restore the new process's context and return
}

/**
 * @brief Set LAPIC reference for EOI
 *
 * Must be called before timer interrupts are enabled.
 *
 * @param lapic Pointer to initialized LAPIC object
 */
void set_timer_lapic(xinim::hal::x86_64::Lapic* lapic) {
    using namespace xinim::kernel;
    g_lapic = lapic;
}

/**
 * @brief Initialize timer subsystem
 *
 * Sets up the APIC timer to fire at the desired frequency.
 * Assumes APIC is already initialized from kernel boot.
 *
 * @param frequency_hz Timer frequency in Hz (default: 100 Hz)
 */
extern "C" void initialize_timer(uint32_t frequency_hz) {
    using namespace xinim::kernel;

    g_timer_ticks = 0;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[TIMER] Initializing at %u Hz\n", frequency_hz);
    early_serial.write(buffer);

    // Timer initialization is done in kernel main (APIC setup)
    // This function just resets state

    if (g_lapic) {
        early_serial.write("[TIMER] LAPIC reference set for EOI\n");
    } else {
        early_serial.write("[WARN] LAPIC not set - EOI will not work!\n");
    }

    early_serial.write("[TIMER] Preemptive scheduling enabled\n");
}

/**
 * @brief Handle unhandled interrupt (for debugging)
 */
extern "C" void handle_unhandled_interrupt() {
    early_serial.write("[FATAL] Unhandled interrupt!\n");
    for(;;) { __asm__ volatile ("hlt"); }
}

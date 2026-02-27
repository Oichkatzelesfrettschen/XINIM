/**
 * @file timer.cpp
 * @brief Timer interrupt handling (Bare-metal refactored)
 */

#include <stdint.h>
#include "early/serial_16550.hpp"
#include "scheduler.hpp"
#include "hal/x86_64/hal/apic.hpp"

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

static xinim::hal::x86_64::Lapic* g_timer_lapic = nullptr;

void set_timer_lapic(xinim::hal::x86_64::Lapic* lapic) {
    g_timer_lapic = lapic;
}

void initialize_timer() {
    early_serial.write("[TIMER] Initialized\n");
}

} // namespace xinim::kernel

extern "C" void timer_interrupt_handler_c() {
    // EOI
    // xinim::kernel::g_timer_lapic->eoi();
    xinim::kernel::schedule();
}

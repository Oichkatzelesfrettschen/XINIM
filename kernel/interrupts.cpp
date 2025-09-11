#include <stdint.h>
#include "arch/x86_64/idt.hpp"
#include "early/serial_16550.hpp"
#include "../arch/x86_64/hal/apic.hpp"

extern "C" void isr_stub_32();

namespace {
    xinim::early::Serial16550* g_serial = nullptr;
    xinim::hal::x86_64::Lapic* g_lapic  = nullptr;
    volatile uint64_t g_ticks = 0;
}

extern "C" void isr_common_handler(uint64_t vec) {
    if (vec == 32) {
        g_ticks++;
        if (g_serial) g_serial->write("tick\n");
        if (g_lapic) g_lapic->eoi();
    }
}

void interrupts_init(xinim::early::Serial16550& serial, xinim::hal::x86_64::Lapic& lapic) {
    using namespace xinim::arch::x86_64::idt;
    g_serial = &serial;
    g_lapic  = &lapic;
    init();
    set_gate(32, isr_stub_32, 0x8E, 0);
}


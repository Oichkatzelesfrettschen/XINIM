/**
 * @file interrupts.cpp
 * @brief Unified interrupt setup and registration.
 *
 * This is the single IDT initialization path. It uses
 * arch::x86_64::idt::init() to create the IDT table and load it via lidt,
 * then installs all interrupt handlers:
 *   - Default handler for all 256 vectors (from mpx64.cpp)
 *   - Timer (vector 32) from interrupts.S
 *   - Clock (CLOCK_VECTOR) from mpx64.cpp
 *   - Keyboard (KEYBOARD_VECTOR) from mpx64.cpp
 *
 * The old idt64.cpp is superseded by this consolidated path.
 */

#include <stdint.h>
#include "arch/x86_64/idt.hpp"
#include "interrupts.hpp"
#include "early/serial_16550.hpp"
#include "../hal/x86_64/hal/apic.hpp"
#include "proc.hpp"
#include "glo.hpp"
#include "sys/com.hpp"

// MINIX heritage ISR handlers (defined in mpx64.cpp)
extern "C" {
    void isr_default() noexcept;
    void isr_clock() noexcept;
    void isr_keyboard() noexcept;
    void s_call() noexcept;
}

extern xinim::early::Serial16550 kshell_serial;

namespace {
    xinim::early::Serial16550* g_serial = nullptr;
}

// C-linkage ISR entry points for COM serial ports.
// Called from the IDT via isr_default or a dedicated stub.
extern "C" void isr_com1() noexcept {
    if (g_serial) {
        g_serial->isr_handler();
    }
}

extern "C" void isr_com2() noexcept {
    kshell_serial.isr_handler();
}

void interrupts_init(xinim::early::Serial16550& serial, [[maybe_unused]] xinim::hal::x86_64::Lapic& lapic) {
    using namespace xinim::arch::x86_64::idt;

    g_serial = &serial;

    // Initialize IDT with zeroed entries and load via lidt
    init();

    // Install default handler for all 256 vectors
    for (int i = 0; i < 256; ++i) {
        set_gate(i, reinterpret_cast<void(*)()>(isr_default), 0x8E, 0);
    }

    // Install specific handlers (overwrite defaults).
    // CLOCK_VECTOR (32) uses the assembly timer_interrupt_handler from interrupts.S
    // which saves context and calls timer_interrupt_c_handler.
    set_gate(CLOCK_VECTOR, timer_interrupt_handler, 0x8E, 0);
    set_gate(KEYBOARD_VECTOR, reinterpret_cast<void(*)()>(isr_keyboard), 0x8E, 0);
    set_gate(SYS_VECTOR, reinterpret_cast<void(*)()>(s_call), 0x8E, 0);

    // Serial port interrupt handlers (COM1=IRQ4, COM2=IRQ3)
    set_gate(COM1_VECTOR, reinterpret_cast<void(*)()>(isr_com1), 0x8E, 0);
    set_gate(COM2_VECTOR, reinterpret_cast<void(*)()>(isr_com2), 0x8E, 0);

    // Enable receive interrupts on both serial ports
    serial.enable_rx_interrupt();
    kshell_serial.enable_rx_interrupt();

    serial.write("[IDT] Unified IDT initialized (256 vectors)\n");
    serial.write("[IDT] Timer, Keyboard, Syscall, COM1, COM2 handlers registered\n");
}

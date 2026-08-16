/**
 * @file interrupts.cpp
 * @brief Unified interrupt setup and registration.
 *
 * This is the single IDT initialization path. It uses
 * arch::x86_64::idt::init() to create the IDT table and load it via lidt,
 * then installs assembly gates that preserve the interrupted state, establish
 * the SysV ABI, call C++ callbacks, and return with IRETQ.
 */

#include "interrupts.hpp"

#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/process_syscalls.hpp"
#include "early/serial_16550.hpp"
#include "glo.hpp"
#include "hal/x86_64/hal/apic.hpp"
#include "proc.hpp"
#include "sys/com.hpp"

#include <stdint.h>

extern "C" void tty_int() noexcept;

extern xinim::early::Serial16550 kshell_serial;

namespace {
    xinim::early::Serial16550 *g_serial = nullptr;
    xinim::hal::x86_64::Lapic *g_lapic = nullptr;

    using ExceptionGate = void (*)();
    constexpr ExceptionGate kExceptionGates[] = {
        x86_64_exception_0,  x86_64_exception_1,  x86_64_exception_2,  x86_64_exception_3,
        x86_64_exception_4,  x86_64_exception_5,  x86_64_exception_6,  x86_64_exception_7,
        x86_64_exception_8,  x86_64_exception_9,  x86_64_exception_10, x86_64_exception_11,
        x86_64_exception_12, x86_64_exception_13, x86_64_exception_14, x86_64_exception_15,
        x86_64_exception_16, x86_64_exception_17, x86_64_exception_18, x86_64_exception_19,
        x86_64_exception_20, x86_64_exception_21, x86_64_exception_22, x86_64_exception_23,
        x86_64_exception_24, x86_64_exception_25, x86_64_exception_26, x86_64_exception_27,
        x86_64_exception_28, x86_64_exception_29, x86_64_exception_30, x86_64_exception_31,
    };

    static_assert(sizeof(kExceptionGates) / sizeof(kExceptionGates[0]) == 32U);

    void send_eoi() noexcept {
        if (g_lapic != nullptr) {
            g_lapic->eoi();
        }
    }

    void write_hex64(xinim::early::Serial16550 &serial, uint64_t value) noexcept {
        constexpr char kHexDigits[] = "0123456789ABCDEF";
        char buffer[19] = "0x0000000000000000";
        for (std::size_t nibble_index = 0; nibble_index < 16U; ++nibble_index) {
            const std::size_t shift = (15U - nibble_index) * 4U;
            const std::size_t digit = static_cast<std::size_t>((value >> shift) & 0xFU);
            buffer[2U + nibble_index] = kHexDigits[digit];
        }
        serial.write(buffer);
    }
} // namespace

extern "C" void keyboard_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *) noexcept {
    tty_int();
    send_eoi();
}

extern "C" void com1_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *) noexcept {
    if (g_serial != nullptr) {
        g_serial->isr_handler();
    }
    send_eoi();
}

extern "C" void com2_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *) noexcept {
    kshell_serial.isr_handler();
    xinim::kernel::x86_64::wake_io_waiters();
    send_eoi();
}

extern "C" void handle_unhandled_irq(const xinim::kernel::X86_64InterruptFrame *) noexcept {
    if (g_serial != nullptr) {
        g_serial->write("[INT] Unhandled interrupt\n");
    }
    send_eoi();
}

extern "C" [[noreturn]] void
handle_unhandled_exception(const xinim::kernel::X86_64ExceptionFrame *frame) noexcept {
    if (g_serial != nullptr) {
        g_serial->write("[EXCEPTION] vector=");
        write_hex64(*g_serial, frame->vector);
        g_serial->write(" error=");
        write_hex64(*g_serial, frame->error_code);
        g_serial->write(" rip=");
        write_hex64(*g_serial, frame->rip);
        g_serial->write("\n");
    }
    for (;;) {
        asm volatile("cli; hlt");
    }
}

void interrupts_init(xinim::early::Serial16550 &serial, xinim::hal::x86_64::Lapic &lapic) {
    using namespace xinim::arch::x86_64::idt;

    g_serial = &serial;
    g_lapic = &lapic;

    // Initialize IDT with zeroed entries and load via lidt
    init();

    // Every installed IDT address is an assembly interrupt gate.
    for (int i = 0; i < 256; ++i) {
        set_gate(i, generic_interrupt_handler, 0x8E, 0);
    }

    // CPU exceptions are processor events, not LAPIC-delivered interrupts.
    for (int vector = 0; vector < 32; ++vector) {
        set_gate(vector, kExceptionGates[vector], 0x8E, 0);
    }

    // Install specific handlers (overwrite defaults).
    set_gate(CLOCK_VECTOR, timer_interrupt_handler, 0x8E, 0);
    set_gate(KEYBOARD_VECTOR, keyboard_interrupt_handler, 0x8E, 0);

    // Serial port interrupt handlers (COM1=IRQ4, COM2=IRQ3)
    set_gate(COM1_VECTOR, com1_interrupt_handler, 0x8E, 0);
    set_gate(COM2_VECTOR, com2_interrupt_handler, 0x8E, 0);
    set_gate(xinim::hal::x86_64::kSpuriousVector, spurious_interrupt_handler, 0x8E, 0);

    // Enable receive interrupts on both serial ports
    serial.enable_rx_interrupt();
    kshell_serial.enable_rx_interrupt();

    serial.write("[IDT] Unified IDT initialized (256 vectors)\n");
    serial.write("[IDT] Timer, Keyboard, COM1, COM2 handlers registered\n");
}

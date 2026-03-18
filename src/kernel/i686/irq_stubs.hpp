#pragma once
// APIC interrupt stubs for i686.
// WHY: The local APIC delivers three categories of interrupt that need IDT
// entries even though the kernel does not yet have full handlers for them:
//
//   0xFF -- Spurious APIC interrupt.
//           The APIC delivers these when an interrupt is retracted before it
//           reaches the CPU.  Per Intel SDM Vol. 3A Section 10.9, the handler
//           MUST NOT send an EOI; just iret.
//
//   0x21 -- IOAPIC IRQ1 (PS/2 keyboard), vector kVectorKeyboard.
//           Keyboard input is polled from the timer IRQ so this handler
//           only needs to acknowledge the interrupt and return.
//
//   0x2E -- IOAPIC IRQ14 (IDE primary), vector kVectorIdePri.
//           IDE is polled; same treatment as keyboard.

namespace xinim::i686 {

// Spurious APIC interrupt stub: iret with no EOI.
extern "C" void i686_apic_spurious_entry() noexcept;

// Generic APIC EOI stub: send EOI to the local APIC then iret.
// Registered for IOAPIC-routed IRQs that have no dedicated handler yet.
extern "C" void i686_apic_irq_eoi_entry() noexcept;

} // namespace xinim::i686

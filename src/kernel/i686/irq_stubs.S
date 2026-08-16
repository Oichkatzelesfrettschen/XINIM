.section .text

// ---------------------------------------------------------------------------
// i686_apic_spurious_entry -- handler for the APIC spurious vector (0xFF).
//
// WHY: Per Intel SDM Vol. 3A Section 10.9 "Spurious Interrupt", the handler
// must NOT write an EOI.  Writing an EOI for a spurious interrupt causes the
// APIC to generate an additional undefined interrupt.  Just iret is correct.
// ---------------------------------------------------------------------------

.global i686_apic_spurious_entry
.type i686_apic_spurious_entry, @function
i686_apic_spurious_entry:
    iret
.size i686_apic_spurious_entry, . - i686_apic_spurious_entry

// ---------------------------------------------------------------------------
// i686_apic_irq_eoi_entry -- stub for IOAPIC-routed IRQs with no handler.
//
// WHY: The kernel uses polling for keyboard (IRQ1) and IDE (IRQ14), so these
// stubs exist only to satisfy the IDT and send an EOI so the APIC does not
// remain in the In-Service state and block subsequent interrupts.
//
// APIC EOI register: physical address 0xFEE000B0 (kApicBase + kRegEoi).
// Writing any value to it signals end-of-interrupt to the local APIC.
// The IOAPIC automatically de-asserts the remote-IRR bit when it sees the
// APIC EOI for an edge-triggered entry (all our IRQs are edge-triggered).
// ---------------------------------------------------------------------------

.global i686_apic_irq_eoi_entry
.type i686_apic_irq_eoi_entry, @function
i686_apic_irq_eoi_entry:
    pushal
    movl    $0x00000000, 0xFEE000B0  // write 0 to APIC EOI register
    popal
    iret
.size i686_apic_irq_eoi_entry, . - i686_apic_irq_eoi_entry

.section .note.GNU-stack,"",@progbits

#pragma once
// Local APIC for i686 (Pentium Pro+).
// WHY: The legacy 8259A PIC works but introduces latency and lacks per-CPU
// timers.  The Local APIC's on-chip timer replaces the PIT channel 0 for
// scheduling, giving us higher resolution and a path toward SMP.
// Registers per Intel SDM Vol. 3A Section 11.4.

#include <stdint.h>

namespace xinim::i686::apic {

// Physical base address of the local APIC MMIO region.
// The local APIC is identity-mapped in kernel virtual address space.
static constexpr uint32_t kApicBase = 0xFEE00000U;

// APIC register offsets (byte offsets from kApicBase).
static constexpr uint32_t kRegId          = 0x020U;
static constexpr uint32_t kRegVersion     = 0x030U;
static constexpr uint32_t kRegTpr         = 0x080U; // task-priority
static constexpr uint32_t kRegEoi         = 0x0B0U; // end-of-interrupt (write 0)
static constexpr uint32_t kRegSpurious    = 0x0F0U; // spurious interrupt vector
static constexpr uint32_t kRegTimerLvt    = 0x320U; // timer local-vector-table entry
static constexpr uint32_t kRegTimerInitCnt= 0x380U; // initial count
static constexpr uint32_t kRegTimerCurCnt = 0x390U; // current count
static constexpr uint32_t kRegTimerDiv    = 0x3E0U; // divide configuration

// LVT timer flags.
static constexpr uint32_t kLvtPeriodic   = (1U << 17U); // periodic mode
static constexpr uint32_t kLvtMasked     = (1U << 16U); // mask interrupt

// Spurious interrupt vector must have bits [3:0] set to 0xF per spec.
static constexpr uint32_t kSpuriousVector = 0x1FFU; // enable=bit8, vector=0xFF

// Timer fires at kTimerVector (0x20=32) at 100 Hz.
// Defined in hw_init.hpp; declared here as extern for documentation.
static constexpr uint8_t kTimerVector = 0x20U;

// Enable the local APIC via the IA32_APIC_BASE MSR (set bit 11) and the
// spurious-interrupt register (set bit 8).  Must be called after CPUID
// confirms has_apic.
void initialize_apic() noexcept;

// Calibrate the APIC timer using PIT channel 2, then program it in
// periodic mode to fire at 100 Hz on kTimerVector.
// Must be called after initialize_apic() and initialize_legacy_pic().
void initialize_apic_timer() noexcept;

// Write 0 to the APIC EOI register to acknowledge an interrupt.
// Replaces outb(PIC1, 0x20) for APIC-routed interrupts.
void send_apic_eoi() noexcept;

// Return this CPU's APIC ID (bits 31:24 of the ID register).
[[nodiscard]] uint8_t apic_id() noexcept;

} // namespace xinim::i686::apic

#pragma once
// IOAPIC for i686 (Pentium Pro+).
// WHY: The 8259A PIC delivers all IRQs to CPU 0 via a single INT line; the
// IOAPIC delivers IRQs individually and supports symmetric multi-processing.
// Even on UP systems the IOAPIC is preferred when the local APIC is in use
// because it replaces the PIC as the interrupt router.
// Registers per Intel 82093AA IOAPIC specification.

#include <stdint.h>

namespace xinim::i686::ioapic {

// Physical base address of the IOAPIC MMIO region (standard firmware default).
static constexpr uint32_t kIoApicBase = 0xFEC00000U;

// IOAPIC indirect register indices.
static constexpr uint32_t kRegId   = 0x00U;
static constexpr uint32_t kRegVer  = 0x01U;
static constexpr uint32_t kRegRte0 = 0x10U; // redirection table entry 0 (lo)

// RTE delivery mode: fixed delivery to local APIC.
static constexpr uint32_t kDeliveryFixed  = 0U;
// RTE trigger mode: edge-triggered (legacy IRQ default).
static constexpr uint32_t kTriggerEdge    = 0U;
// RTE polarity: active-high (legacy IRQ default).
static constexpr uint32_t kPolarityHigh   = 0U;
// RTE mask bit.
static constexpr uint32_t kRteMasked      = (1U << 16U);

// Destination field encoding for physical destination mode: APIC ID 0.
static constexpr uint32_t kDestCpu0 = 0U; // APIC ID 0 in physical mode

// IRQ -> vector assignments for the IRQs we route.
static constexpr uint8_t kVectorKeyboard = 0x21U; // IRQ 1  -> vector 33
static constexpr uint8_t kVectorIdePri   = 0x2EU; // IRQ 14 -> vector 46

// Program the IOAPIC to route the IRQs the kernel uses and mask everything
// else.  Must be called after initialize_apic().
void initialize_ioapic() noexcept;

// Change the mask bit for a single IRQ.  Used to re-mask after spurious IRQ.
void ioapic_set_mask(uint8_t irq, bool masked) noexcept;

} // namespace xinim::i686::ioapic

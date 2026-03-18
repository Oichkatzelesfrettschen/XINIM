#include "ioapic.hpp"

namespace xinim::i686::ioapic {

// ---------------------------------------------------------------------------
// Indirect register access through IOREGSEL (offset 0x00) + IOWIN (offset 0x10).
// ---------------------------------------------------------------------------

static void ioapic_write(uint32_t reg, uint32_t value) noexcept {
    *reinterpret_cast<volatile uint32_t*>(kIoApicBase + 0x00U) = reg;
    *reinterpret_cast<volatile uint32_t*>(kIoApicBase + 0x10U) = value;
}

static uint32_t ioapic_read(uint32_t reg) noexcept {
    *reinterpret_cast<volatile uint32_t*>(kIoApicBase + 0x00U) = reg;
    return *reinterpret_cast<volatile uint32_t*>(kIoApicBase + 0x10U);
}

// ---------------------------------------------------------------------------
// Redirection Table Entry (RTE) helpers.
// Each RTE occupies two consecutive 32-bit registers starting at kRegRte0 + irq*2.
// Low dword:  [7:0]=vector [10:8]=delivery [11]=destmode [12]=pending [13]=polarity
//             [14]=remoteirr [15]=trigger [16]=mask [18:17]=timer mode
// High dword: [31:24]=destination (physical mode = APIC ID)
// ---------------------------------------------------------------------------

static void rte_write(uint8_t irq, uint32_t lo, uint32_t hi) noexcept {
    const uint32_t base = kRegRte0 + static_cast<uint32_t>(irq) * 2U;
    ioapic_write(base + 1U, hi); // write hi first to avoid transient unmasked state
    ioapic_write(base,      lo);
}

static void rte_set_masked(uint8_t irq, bool masked) noexcept {
    const uint32_t base = kRegRte0 + static_cast<uint32_t>(irq) * 2U;
    uint32_t lo = ioapic_read(base);
    if (masked) {
        lo |= kRteMasked;
    } else {
        lo &= ~kRteMasked;
    }
    ioapic_write(base, lo);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initialize_ioapic() noexcept {
    // Query maximum redirection table entry count.
    const uint32_t ver = ioapic_read(kRegVer);
    const uint8_t max_rte = static_cast<uint8_t>((ver >> 16U) & 0xFFU);

    // Mask all entries first.
    for (uint8_t i = 0U; i <= max_rte; ++i) {
        rte_write(i,
            kRteMasked | kDeliveryFixed | kTriggerEdge | kPolarityHigh,
            kDestCpu0 << 24U);
    }

    // IRQ 1: PS/2 keyboard -> vector 0x21, edge, active-high, CPU 0.
    rte_write(1U,
        static_cast<uint32_t>(kVectorKeyboard)
            | (kDeliveryFixed << 8U)
            | kTriggerEdge
            | kPolarityHigh,
        kDestCpu0 << 24U);

    // IRQ 14: IDE primary -> vector 0x2E, edge, active-high, CPU 0.
    rte_write(14U,
        static_cast<uint32_t>(kVectorIdePri)
            | (kDeliveryFixed << 8U)
            | kTriggerEdge
            | kPolarityHigh,
        kDestCpu0 << 24U);
}

void ioapic_set_mask(uint8_t irq, bool masked) noexcept {
    rte_set_masked(irq, masked);
}

} // namespace xinim::i686::ioapic

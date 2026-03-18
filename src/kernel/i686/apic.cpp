#include "apic.hpp"
#include "../i486/hw_init.hpp" // outb/inb_port, kPitInputHz

namespace xinim::i686::apic {

// ---------------------------------------------------------------------------
// MMIO helpers -- the APIC registers are 32-bit, DWORD aligned.
// ---------------------------------------------------------------------------

static uint32_t apic_read(uint32_t offset) noexcept {
    return *reinterpret_cast<volatile uint32_t*>(kApicBase + offset);
}

static void apic_write(uint32_t offset, uint32_t value) noexcept {
    *reinterpret_cast<volatile uint32_t*>(kApicBase + offset) = value;
}

// ---------------------------------------------------------------------------
// IA32_APIC_BASE MSR
// ---------------------------------------------------------------------------

static constexpr uint32_t kMsrApicBase = 0x1BU;

static uint64_t rdmsr(uint32_t msr) noexcept {
    uint32_t lo = 0U;
    uint32_t hi = 0U;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return (static_cast<uint64_t>(hi) << 32U) | lo;
}

static void wrmsr(uint32_t msr, uint64_t value) noexcept {
    __asm__ volatile("wrmsr"
                     :: "c"(msr),
                        "a"(static_cast<uint32_t>(value & 0xFFFFFFFFU)),
                        "d"(static_cast<uint32_t>(value >> 32U)));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initialize_apic() noexcept {
    // Enable APIC globally via IA32_APIC_BASE MSR bit 11.
    const uint64_t base = rdmsr(kMsrApicBase);
    wrmsr(kMsrApicBase, base | (1U << 11U));

    // Set task-priority register to 0 to allow all interrupts.
    apic_write(kRegTpr, 0U);

    // Enable local APIC by setting bit 8 in the spurious-interrupt vector
    // register.  Vector 0xFF satisfies the hardware requirement that bits[3:0]
    // are all set.
    apic_write(kRegSpurious, kSpuriousVector);
}

void initialize_apic_timer() noexcept {
    // Divide by 16 -- a moderate pre-scaler for PIT calibration accuracy.
    // Divisor encoding: 0b0011 = divide-by-16 (SDM Table 11-2).
    apic_write(kRegTimerDiv, 0x3U);

    // Mask the APIC timer while calibrating.
    apic_write(kRegTimerLvt, kLvtMasked | static_cast<uint32_t>(kTimerVector));

    // Calibrate: count APIC ticks in 10 ms using PIT channel 2.
    // PIT channel 2 in one-shot mode (mode 0).
    // Load 10ms worth of PIT ticks: 10ms * 1193182 Hz = 11932 ticks.
    constexpr uint16_t kPitTicks10ms = static_cast<uint16_t>(
        xinim::i486::ring3::kPitInputHz / 100U);

    // Gate PIT channel 2 (port 0x61 bit 0=gate, bit 1=speaker).
    // Clear gate first, then load counter, then enable gate.
    xinim::i486::ring3::outb(0x61U,
        static_cast<uint8_t>(xinim::i486::ring3::inb_port(0x61U) & 0xFCU));
    // Mode/command: channel 2, lo/hi byte, mode 0 (one-shot), binary.
    xinim::i486::ring3::outb(0x43U, 0xB0U);
    xinim::i486::ring3::outb(0x42U, static_cast<uint8_t>(kPitTicks10ms & 0xFFU));
    xinim::i486::ring3::outb(0x42U, static_cast<uint8_t>(kPitTicks10ms >> 8U));

    // Start APIC timer at max count.
    apic_write(kRegTimerInitCnt, 0xFFFFFFFFU);

    // Start PIT channel 2 gate.
    xinim::i486::ring3::outb(0x61U,
        static_cast<uint8_t>(xinim::i486::ring3::inb_port(0x61U) | 0x01U));

    // Wait for PIT channel 2 OUT to go high (bit 5 of port 0x61).
    while ((xinim::i486::ring3::inb_port(0x61U) & 0x20U) == 0U) {}

    // Read remaining APIC counter; ticks elapsed in 10ms.
    const uint32_t remaining = apic_read(kRegTimerCurCnt);
    const uint32_t ticks_per_10ms = 0xFFFFFFFFU - remaining;

    // Scale to 100 Hz: 10 * ticks_per_10ms ticks per second -> ticks_per_10ms
    // per tick at 100 Hz.
    apic_write(kRegTimerLvt, kLvtPeriodic | static_cast<uint32_t>(kTimerVector));
    apic_write(kRegTimerDiv, 0x3U);
    apic_write(kRegTimerInitCnt, ticks_per_10ms != 0U ? ticks_per_10ms : 100000U);
}

void send_apic_eoi() noexcept {
    apic_write(kRegEoi, 0U);
}

uint8_t apic_id() noexcept {
    return static_cast<uint8_t>(apic_read(kRegId) >> 24U);
}

} // namespace xinim::i686::apic

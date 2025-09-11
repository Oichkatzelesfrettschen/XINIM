#include "apic.hpp"

namespace xinim::hal::x86_64 {

static inline void mmio_write(volatile uint32_t* base, uint32_t reg, uint32_t val) {
    base[reg/4] = val;
}
static inline uint32_t mmio_read(volatile uint32_t* base, uint32_t reg) {
    return base[reg/4];
}

void Lapic::init(uintptr_t mmio_base) {
    base_ = reinterpret_cast<volatile uint32_t*>(mmio_base);
    // Spurious interrupt vector register at 0xF0: enable bit (8)
    mmio_write(base_, 0xF0, (0xFF /*vector*/)| (1u<<8));
}

void Lapic::eoi() {
    mmio_write(base_, 0xB0, 0);
}

void Lapic::setup_timer(uint8_t vector, uint32_t initial_count, uint8_t divide_power_of_two, bool periodic) {
    // Divide Configuration Register 0x3E0
    // Encode divide by 2^n where n in {1,2,3,4,5,6,7,8} mapping to specific bits.
    // For common values, 16 -> 0x3
    uint32_t div = 0x3;
    if (divide_power_of_two == 1) div = 0x0;
    else if (divide_power_of_two == 2) div = 0x1;
    else if (divide_power_of_two == 3) div = 0x2;
    else if (divide_power_of_two == 4) div = 0x3; // 16
    mmio_write(base_, 0x3E0, div);
    // LVT Timer 0x320: set vector and mode
    uint32_t lvt = vector;
    if (periodic) lvt |= (1u<<17);
    mmio_write(base_, 0x320, lvt);
    // Initial Count 0x380
    mmio_write(base_, 0x380, initial_count);
}

// Helper methods for calibration (not exposed in header): read current count
[[maybe_unused]] static uint32_t lapic_current_count(volatile uint32_t* base) {
    return mmio_read(base, 0x390);
}

uint32_t Lapic::current_count() const {
    return mmio_read(base_, 0x390);
}

void Lapic::stop_timer() {
    // Mask LVT timer (bit 16)
    uint32_t lvt = mmio_read(base_, 0x320);
    lvt |= (1u<<16);
    mmio_write(base_, 0x320, lvt);
    // Clear initial count
    mmio_write(base_, 0x380, 0);
}


} // namespace xinim::hal::x86_64

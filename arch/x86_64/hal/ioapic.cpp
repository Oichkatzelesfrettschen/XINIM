#include "ioapic.hpp"

namespace xinim::hal::x86_64 {

void IoApic::init(uintptr_t mmio_base, uint32_t gsi_base) {
    base_ = reinterpret_cast<volatile uint32_t*>(mmio_base);
    gsi_base_ = gsi_base;
}

void IoApic::write(uint8_t reg, uint32_t value) {
    base_[0] = reg;
    base_[4] = value;
}

uint32_t IoApic::read(uint8_t reg) {
    base_[0] = reg;
    return base_[4];
}

void IoApic::redirect(uint32_t gsi, uint8_t vector, bool level, bool active_low) {
    uint32_t idx = (gsi - gsi_base_) * 2 + 0x10;
    uint32_t low = vector;
    if (active_low) low |= (1u<<13);
    if (level) low |= (1u<<15);
    // Unmask (bit 16 = 0)
    write(idx, low);
    write(idx+1, 0); // destination CPU ID 0
}

}


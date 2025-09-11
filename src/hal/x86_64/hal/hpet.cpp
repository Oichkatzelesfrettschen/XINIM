#include "hpet.hpp"

namespace xinim::hal::x86_64 {

void Hpet::init(uintptr_t mmio_base) {
    base_ = reinterpret_cast<volatile uint64_t*>(mmio_base);
    // General Capabilities (offset 0x0): bits 63:32 period in femtoseconds
    const uint64_t caps = base_[0];
    period_fs_ = caps >> 32;
}

uint64_t Hpet::counter() const {
    // Main counter at offset 0xF0 / 8
    return base_[0xF0/8];
}

void Hpet::enable(bool en) {
    // General Configuration at 0x10 / 8
    volatile uint64_t* cfg = base_ + (0x10/8);
    uint64_t v = *cfg;
    if (en) v |= 1; else v &= ~1ULL;
    *cfg = v;
}

bool Hpet::start_periodic(uint32_t timer, uint64_t per_ns, uint32_t route_gsi) {
    if (!base_ || period_fs_ == 0) return false;
    // Compute comparator ticks from ns
    __uint128_t ticks = ((__uint128_t)per_ns * 1000000ULL) / (__uint128_t)period_fs_;
    if (ticks == 0) ticks = 1;
    // Timer N config/comparator offsets
    volatile uint64_t* tconf = base_ + ((0x100 + 0x20 * timer)/8);
    volatile uint64_t* tcomp = base_ + ((0x108 + 0x20 * timer)/8);
    uint64_t cfg = *tconf;
    // Set interrupt enable (bit 2), periodic (bit 3), edge triggered (bit 1 = 0)
    cfg |= (1ULL<<2) | (1ULL<<3);
    // Program interrupt route (implementation-defined field width; common HPET uses bits 9..13)
    // Clear previous route bits and set a subset of bits with route_gsi
    cfg &= ~(((uint64_t)0x1F) << 9);
    cfg |= (((uint64_t)(route_gsi & 0x1F)) << 9);
    *tconf = cfg;
    *tcomp = (uint64_t)ticks;
    return true;
}

} // namespace xinim::hal::x86_64

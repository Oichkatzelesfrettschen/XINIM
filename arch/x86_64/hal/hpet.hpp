#pragma once
#include <stdint.h>

namespace xinim::hal::x86_64 {

class Hpet {
  public:
    void init(uintptr_t mmio_base);
    uint64_t counter() const;
    uint64_t period_fs() const { return period_fs_; }
    void enable(bool en=true);
    uint64_t period_ns() const { return period_fs_ / 1000000ULL; }
    // Program timer N for periodic interrupts; route_gsi is the desired IOAPIC GSI line
    bool start_periodic(uint32_t timer, uint64_t period_ns, uint32_t route_gsi);
  private:
    volatile uint64_t* base_{nullptr};
    uint64_t period_fs_{0};
};

} // namespace xinim::hal::x86_64

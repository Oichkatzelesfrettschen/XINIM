#pragma once
#include <stdint.h>

namespace xinim::hal::x86_64 {

class Lapic {
  public:
    void init(uintptr_t mmio_base);
    void eoi();
    void setup_timer(uint8_t vector, uint32_t initial_count, uint8_t divide_power_of_two = 4 /*16*/, bool periodic=true);
    uint32_t current_count() const;
    void stop_timer();
  private:
    volatile uint32_t* base_{nullptr};
};

} // namespace xinim::hal::x86_64

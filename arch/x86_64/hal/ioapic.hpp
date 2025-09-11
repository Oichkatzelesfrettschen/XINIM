#pragma once
#include <stdint.h>

namespace xinim::hal::x86_64 {

class IoApic {
  public:
    void init(uintptr_t mmio_base, uint32_t gsi_base);
    void redirect(uint32_t gsi, uint8_t vector, bool level=false, bool active_low=false);
  private:
    volatile uint32_t* base_{nullptr};
    uint32_t gsi_base_{0};
    void write(uint8_t reg, uint32_t value);
    uint32_t read(uint8_t reg);
};

}


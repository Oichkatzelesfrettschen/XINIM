#pragma once
#include <stdint.h>

namespace xinim::hal::x86_64 {

class Pci {
  public:
    static uint32_t cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
};

} // namespace xinim::hal::x86_64

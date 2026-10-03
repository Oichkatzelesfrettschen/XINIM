// ISA-independent CPU interface
#pragma once
#include <stdint.h>

namespace xinim::hal {

struct CpuId {
    uint32_t eax{}, ebx{}, ecx{}, edx{};
};

class Cpu {
  public:
    virtual ~Cpu() = default;
    virtual CpuId cpuid(uint32_t leaf, uint32_t subleaf = 0) noexcept = 0;
    virtual void pause() noexcept = 0;
    virtual uint64_t rdtsc() noexcept = 0;
    virtual void enable_interrupts() noexcept = 0;
    virtual void disable_interrupts() noexcept = 0;
};

} // namespace xinim::hal

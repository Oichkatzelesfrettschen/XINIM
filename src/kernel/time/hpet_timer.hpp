#pragma once
#include <stdint.h>
#include "../../arch/x86_64/hal/hpet.hpp"
#include "../../arch/x86_64/hal/apic.hpp"
#include <xinim/hal/timer.hpp>

namespace xinim::time {

class HpetTimer : public xinim::hal::Timer {
  public:
    HpetTimer(xinim::hal::x86_64::Hpet& hpet, xinim::hal::x86_64::Lapic& lapic, uint8_t vector)
      : hpet_(hpet), lapic_(lapic), vector_(vector) {}
    void init() override {}
    uint64_t monotonic_ns() const override {
        const uint64_t period_fs = hpet_.period_fs();
        const uint64_t ticks = hpet_.counter();
        // ns = ticks * period_fs / 1e6
        return (uint64_t)(((__uint128_t)ticks * (__uint128_t)period_fs) / 1000000ULL);
    }
    void oneshot_after_ns(uint64_t ns) override {
        // Rough conversion: pick divider 16, convert ns to APIC count via HPET timebase
        uint64_t count = (ns / 1000ULL); // simplistic placeholder; calibrator is preferred
        if (count == 0) count = 1;
        lapic_.setup_timer(vector_, (uint32_t)count, 4, false);
    }
  private:
    xinim::hal::x86_64::Hpet& hpet_;
    xinim::hal::x86_64::Lapic& lapic_;
    uint8_t vector_;
};

}


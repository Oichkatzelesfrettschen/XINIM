#pragma once
#include <stdint.h>
#include "../../arch/x86_64/hal/apic.hpp"
#include "../../arch/x86_64/hal/hpet.hpp"

namespace xinim::time {

struct ApicCalibResult {
    uint32_t initial_count;
    uint8_t  divider_pow2; // power of two (e.g., 4 -> divide by 16)
};

// Calibrate APIC timer using HPET as reference for desired_hz.
ApicCalibResult calibrate_apic_with_hpet(xinim::hal::x86_64::Lapic& lapic,
                                         xinim::hal::x86_64::Hpet& hpet,
                                         uint32_t desired_hz);

}


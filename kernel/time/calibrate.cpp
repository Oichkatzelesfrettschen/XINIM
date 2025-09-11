#include "calibrate.hpp"

namespace xinim::time {

ApicCalibResult calibrate_apic_with_hpet(xinim::hal::x86_64::Lapic& lapic,
                                         xinim::hal::x86_64::Hpet& hpet,
                                         uint32_t desired_hz) {
    ApicCalibResult r{0, 4}; // divide by 16 default
    const uint64_t period_fs = hpet.period_fs();
    if (!period_fs || !desired_hz) return r;

    // Use HPET to measure APIC timer rate in one-shot mode.
    // Target a 10ms sample window.
    const uint64_t sample_ns = 10'000'000ULL;
    const uint32_t trial_initial = 50'000'000U; // arbitrary large-ish
    lapic.setup_timer(32, trial_initial, r.divider_pow2, false /*one-shot*/);

    uint64_t start = hpet.counter();
    // Busy wait until sample_ns elapses
    for (;;) {
        uint64_t now = hpet.counter();
        __uint128_t ns = ((__uint128_t)(now - start) * (__uint128_t)period_fs) / 1000000ULL;
        if (ns >= sample_ns) break;
    }

    // Measure elapsed APIC ticks over sample_ns via current_count
    uint32_t remaining = lapic.current_count();
    uint32_t elapsed_apic = trial_initial - remaining;
    if (elapsed_apic == 0) elapsed_apic = 1;
    __uint128_t ticks_per_ns = ( (__uint128_t)elapsed_apic ) / sample_ns;
    if (ticks_per_ns == 0) return r;
    __uint128_t count_for_period = ( (__uint128_t)1'000'000'000ULL * ticks_per_ns ) / desired_hz;
    if (count_for_period == 0) count_for_period = 1;
    r.initial_count = (uint32_t)count_for_period;
    return r;
}

}

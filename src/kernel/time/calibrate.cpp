#include "calibrate.hpp"

#include "apic_calibration_math.hpp"

namespace xinim::time {

    ApicCalibResult calibrate_apic_with_hpet(xinim::hal::x86_64::Lapic &lapic,
                                             xinim::hal::x86_64::Hpet &hpet, uint32_t desired_hz) {
        ApicCalibResult r{0, 4}; // divide by 16 default
        const uint64_t period_fs = hpet.period_fs();
        if (!period_fs || !desired_hz)
            return r;

        // Use HPET to measure APIC timer rate in one-shot mode.  Read both counters
        // at each boundary so LAPIC programming latency is excluded from the
        // measured decrement.  A 10 ms interval is long enough to make MMIO read
        // skew insignificant while remaining bounded during early boot.
        const uint64_t sample_ns = 10'000'000ULL;
        const uint64_t target_hpet_ticks =
            ((sample_ns * 1'000'000ULL) + period_fs - 1U) / period_fs;
        const uint32_t trial_initial = 50'000'000U; // arbitrary large-ish
        lapic.setup_timer(32, trial_initial, r.divider_pow2, false /*one-shot*/,
                          true /*masked until the IDT is installed*/);

        const uint64_t start_hpet = hpet.counter();
        const uint32_t start_apic = lapic.current_count();
        uint64_t end_hpet = start_hpet;
        constexpr uint32_t kMaximumCounterReads = 200'000U;
        for (uint32_t counter_reads = 0U; counter_reads < kMaximumCounterReads; ++counter_reads) {
            end_hpet = hpet.counter();
            if ((end_hpet - start_hpet) >= target_hpet_ticks) {
                break;
            }
        }

        const uint64_t elapsed_hpet_ticks = end_hpet - start_hpet;
        if (elapsed_hpet_ticks < target_hpet_ticks) {
            lapic.stop_timer();
            return r;
        }

        const uint32_t end_apic = lapic.current_count();
        if (end_apic > start_apic) {
            lapic.stop_timer();
            return r;
        }
        const uint32_t elapsed_apic_ticks = start_apic - end_apic;
        r.initial_count = compute_apic_timer_initial_count(elapsed_apic_ticks, elapsed_hpet_ticks,
                                                           period_fs, desired_hz);
        return r;
    }

} // namespace xinim::time

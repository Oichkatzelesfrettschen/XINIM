#pragma once

#include <cstdint>

namespace xinim::time {

    [[nodiscard]] constexpr uint32_t
    compute_apic_timer_initial_count(uint32_t elapsed_apic_ticks, uint64_t elapsed_hpet_ticks,
                                     uint64_t hpet_period_fs, uint32_t desired_hz) noexcept {
        if (elapsed_apic_ticks == 0U || elapsed_hpet_ticks == 0U || hpet_period_fs == 0U ||
            desired_hz == 0U) {
            return 0U;
        }

        const __uint128_t elapsed_fs =
            static_cast<__uint128_t>(elapsed_hpet_ticks) * hpet_period_fs;
        const __uint128_t denominator = elapsed_fs * desired_hz;
        if (denominator == 0U) {
            return 0U;
        }

        constexpr __uint128_t kFemtosecondsPerSecond = 1'000'000'000'000'000ULL;
        const __uint128_t numerator =
            static_cast<__uint128_t>(elapsed_apic_ticks) * kFemtosecondsPerSecond;
        const __uint128_t rounded_count = (numerator + (denominator / 2U)) / denominator;
        if (rounded_count == 0U || rounded_count > UINT32_MAX) {
            return 0U;
        }
        return static_cast<uint32_t>(rounded_count);
    }

} // namespace xinim::time

#pragma once

#include <cstdint>
#include <xinim/kernel_limits.hpp>

namespace xinim::kernel::sched_policy {

    inline constexpr int NUM_PRIORITIES = 64;
    inline constexpr int MAX_PROCESSES = static_cast<int>(xinim::limits::MAX_PROCESS_COUNT);

    inline constexpr uint32_t PRIO_INTERRUPT = 0;
    inline constexpr uint32_t PRIO_SYSTEM_LO = 1;
    inline constexpr uint32_t PRIO_SYSTEM_HI = 3;
    inline constexpr uint32_t PRIO_SERVER_LO = 4;
    inline constexpr uint32_t PRIO_SERVER_HI = 7;
    inline constexpr uint32_t PRIO_USER_HIGH = 8;
    inline constexpr uint32_t PRIO_USER_NORM = 16;
    inline constexpr uint32_t PRIO_USER_LOW = 32;
    inline constexpr uint32_t PRIO_IDLE = 48;

    inline constexpr uint64_t PRIORITY_REBALANCE_PERIOD_TICKS = 64;

    inline constexpr uint32_t clamp_priority(uint32_t priority) noexcept {
        return priority < static_cast<uint32_t>(NUM_PRIORITIES)
                   ? priority
                   : static_cast<uint32_t>(NUM_PRIORITIES - 1);
    }

    inline constexpr uint32_t quantum_for_priority(uint32_t priority) noexcept {
        const uint32_t clamped = clamp_priority(priority);
        if (clamped <= 3U) {
            return 0U;
        }
        if (clamped <= 7U) {
            return 20U;
        }
        if (clamped <= 15U) {
            return 10U;
        }
        if (clamped <= 31U) {
            return 8U;
        }
        if (clamped <= 47U) {
            return 4U;
        }
        return 1U;
    }

    inline constexpr bool is_preemptible_priority(uint32_t priority) noexcept {
        return quantum_for_priority(priority) != 0U;
    }

    inline constexpr bool should_demote_on_quantum_expiry(uint32_t priority) noexcept {
        const uint32_t clamped = clamp_priority(priority);
        return clamped < static_cast<uint32_t>(NUM_PRIORITIES - 1) && clamped < PRIO_IDLE;
    }

    inline constexpr uint32_t demote_priority(uint32_t priority) noexcept {
        const uint32_t clamped = clamp_priority(priority);
        if (!should_demote_on_quantum_expiry(clamped)) {
            return clamped;
        }
        return clamped + 1U;
    }

    inline constexpr uint32_t rebalance_toward_base(uint32_t priority,
                                                    uint32_t base_priority) noexcept {
        const uint32_t clamped_priority = clamp_priority(priority);
        const uint32_t clamped_base = clamp_priority(base_priority);
        if (clamped_priority <= clamped_base) {
            return clamped_priority;
        }
        return clamped_priority - 1U;
    }

} // namespace xinim::kernel::sched_policy

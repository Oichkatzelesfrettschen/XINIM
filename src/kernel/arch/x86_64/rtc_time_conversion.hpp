#pragma once

#include <cstdint>

namespace xinim::kernel::x86_64 {

    struct RtcSnapshot {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;
        uint8_t month;
        uint8_t year;
        uint8_t century;
    };

    [[nodiscard]] bool rtc_snapshot_to_epoch(RtcSnapshot snapshot, uint8_t status_register_b,
                                             uint64_t &epoch) noexcept;

} // namespace xinim::kernel::x86_64

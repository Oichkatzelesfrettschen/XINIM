#include "rtc_time_conversion.hpp"

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint8_t kBinaryMode = 0x04U;
        constexpr uint8_t kTwentyFourHourMode = 0x02U;

        [[nodiscard]] constexpr bool valid_bcd(uint8_t value) noexcept {
            return (value & 0x0fU) <= 9U && ((value >> 4U) & 0x0fU) <= 9U;
        }

        [[nodiscard]] constexpr uint8_t bcd_to_binary(uint8_t value) noexcept {
            return static_cast<uint8_t>((value & 0x0fU) + ((value >> 4U) * 10U));
        }

        [[nodiscard]] constexpr bool leap_year(uint32_t year) noexcept {
            return (year % 4U) == 0U && ((year % 100U) != 0U || (year % 400U) == 0U);
        }

        [[nodiscard]] constexpr uint32_t days_in_month(uint32_t month, bool leap) noexcept {
            constexpr uint8_t month_days[12] = {
                31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U,
            };
            if (month < 1U || month > 12U) {
                return 0U;
            }
            return month == 2U && leap ? 29U : month_days[month - 1U];
        }

        [[nodiscard]] constexpr uint32_t days_before_month(uint32_t month, bool leap) noexcept {
            constexpr uint16_t cumulative_days[12] = {
                0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U,
            };
            if (month < 1U || month > 12U) {
                return 0U;
            }
            uint32_t days = cumulative_days[month - 1U];
            if (leap && month > 2U) {
                ++days;
            }
            return days;
        }

    } // namespace

    bool rtc_snapshot_to_epoch(RtcSnapshot snapshot, uint8_t status_register_b,
                               uint64_t &epoch) noexcept {
        const bool binary = (status_register_b & kBinaryMode) != 0U;
        const bool twenty_four_hour = (status_register_b & kTwentyFourHourMode) != 0U;
        const bool afternoon = (snapshot.hour & 0x80U) != 0U;
        snapshot.hour &= 0x7fU;
        if (!binary) {
            if (!valid_bcd(snapshot.second) || !valid_bcd(snapshot.minute) ||
                !valid_bcd(snapshot.hour) || !valid_bcd(snapshot.day) ||
                !valid_bcd(snapshot.month) || !valid_bcd(snapshot.year) ||
                !valid_bcd(snapshot.century)) {
                return false;
            }
            snapshot.second = bcd_to_binary(snapshot.second);
            snapshot.minute = bcd_to_binary(snapshot.minute);
            snapshot.hour = bcd_to_binary(snapshot.hour);
            snapshot.day = bcd_to_binary(snapshot.day);
            snapshot.month = bcd_to_binary(snapshot.month);
            snapshot.year = bcd_to_binary(snapshot.year);
            snapshot.century = bcd_to_binary(snapshot.century);
        }
        if (!twenty_four_hour) {
            if (snapshot.hour < 1U || snapshot.hour > 12U) {
                return false;
            }
            snapshot.hour = static_cast<uint8_t>(snapshot.hour % 12U);
            if (afternoon) {
                snapshot.hour = static_cast<uint8_t>(snapshot.hour + 12U);
            }
        }
        const uint32_t century = snapshot.century != 0U ? snapshot.century : 20U;
        const uint32_t year = century * 100U + snapshot.year;
        const bool leap = leap_year(year);
        if (year < 1970U || snapshot.month < 1U || snapshot.month > 12U || snapshot.day < 1U ||
            snapshot.day > days_in_month(snapshot.month, leap) || snapshot.hour > 23U ||
            snapshot.minute > 59U || snapshot.second > 59U) {
            return false;
        }
        uint64_t days = 0U;
        for (uint32_t current_year = 1970U; current_year < year; ++current_year) {
            days += leap_year(current_year) ? 366U : 365U;
        }
        days += days_before_month(snapshot.month, leap);
        days += snapshot.day - 1U;
        epoch = days * 86400U + static_cast<uint64_t>(snapshot.hour) * 3600U +
                static_cast<uint64_t>(snapshot.minute) * 60U + snapshot.second;
        return true;
    }

} // namespace xinim::kernel::x86_64

#include "arch/x86_64/rtc_time_conversion.hpp"

#include <cassert>
#include <cstdint>

namespace {

    using xinim::kernel::x86_64::rtc_snapshot_to_epoch;
    using xinim::kernel::x86_64::RtcSnapshot;

    void require_epoch(RtcSnapshot snapshot, uint8_t status_register_b, uint64_t expected) {
        uint64_t actual = 0U;
        assert(rtc_snapshot_to_epoch(snapshot, status_register_b, actual));
        assert(actual == expected);
    }

    void test_binary_twenty_four_hour_time() {
        require_epoch({0U, 0U, 0U, 1U, 1U, 70U, 19U}, 0x06U, 0U);
        require_epoch({56U, 34U, 12U, 29U, 2U, 24U, 20U}, 0x06U, 1709210096U);
    }

    void test_bcd_twelve_hour_time() {
        require_epoch({0x00U, 0x00U, 0x12U, 0x01U, 0x01U, 0x70U, 0x19U}, 0x00U, 0U);
        require_epoch({0x00U, 0x00U, 0x92U, 0x01U, 0x01U, 0x70U, 0x19U}, 0x00U, 43200U);
    }

    void test_invalid_calendar_values() {
        uint64_t epoch = 0U;
        assert(!rtc_snapshot_to_epoch({0U, 0U, 0U, 29U, 2U, 23U, 20U}, 0x06U, epoch));
        assert(!rtc_snapshot_to_epoch({0U, 0U, 0U, 31U, 4U, 24U, 20U}, 0x06U, epoch));
        assert(!rtc_snapshot_to_epoch({0x6aU, 0U, 0U, 1U, 1U, 0x24U, 0x20U}, 0x02U, epoch));
    }

} // namespace

int main() {
    test_binary_twenty_four_hour_time();
    test_bcd_twelve_hour_time();
    test_invalid_calendar_values();
    return 0;
}

#include "realtime_clock.hpp"

#include "../../timer.hpp"
#include "../../unified_scheduler.hpp"
#include "rtc_time_conversion.hpp"

#include <cstdint>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint16_t kCmosIndexPort = 0x70U;
        constexpr uint16_t kCmosDataPort = 0x71U;
        constexpr uint8_t kUpdateInProgress = 0x80U;
        constexpr uint8_t kStatusRegisterA = 0x0aU;
        constexpr uint8_t kStatusRegisterB = 0x0bU;

        uint64_t g_boot_epoch_seconds = 0U;
        uint64_t g_boot_tick = 0U;
        bool g_clock_initialized = false;

        void out_byte(uint16_t port, uint8_t value) noexcept {
            asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
        }

        [[nodiscard]] uint8_t in_byte(uint16_t port) noexcept {
            uint8_t value = 0U;
            asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
            return value;
        }

        [[nodiscard]] uint8_t read_cmos(uint8_t reg) noexcept {
            out_byte(kCmosIndexPort, static_cast<uint8_t>(0x80U | reg));
            const uint8_t value = in_byte(kCmosDataPort);
            out_byte(kCmosIndexPort, 0U);
            return value;
        }

        [[nodiscard]] bool update_in_progress() noexcept {
            return (read_cmos(kStatusRegisterA) & kUpdateInProgress) != 0U;
        }

        [[nodiscard]] RtcSnapshot read_snapshot() noexcept {
            return {
                read_cmos(0x00U), read_cmos(0x02U), read_cmos(0x04U), read_cmos(0x07U),
                read_cmos(0x08U), read_cmos(0x09U), read_cmos(0x32U),
            };
        }

        [[nodiscard]] bool equal(const RtcSnapshot &left, const RtcSnapshot &right) noexcept {
            return left.second == right.second && left.minute == right.minute &&
                   left.hour == right.hour && left.day == right.day && left.month == right.month &&
                   left.year == right.year && left.century == right.century;
        }

    } // namespace

    bool initialize_realtime_clock() noexcept {
        RtcSnapshot first{};
        RtcSnapshot second{};
        do {
            while (update_in_progress()) {
                asm volatile("pause");
            }
            first = read_snapshot();
            while (update_in_progress()) {
                asm volatile("pause");
            }
            second = read_snapshot();
        } while (!equal(first, second));

        uint64_t epoch = 0U;
        const uint8_t status_register_b = read_cmos(kStatusRegisterB);
        if (!rtc_snapshot_to_epoch(second, status_register_b, epoch)) {
            return false;
        }
        g_boot_epoch_seconds = epoch;
        g_boot_tick = g_unified_scheduler.tick_count();
        g_clock_initialized = true;
        return true;
    }

    uint64_t realtime_seconds() noexcept {
        if (!g_clock_initialized) {
            static_cast<void>(initialize_realtime_clock());
        }
        const uint64_t elapsed = g_unified_scheduler.tick_count() - g_boot_tick;
        return g_boot_epoch_seconds + elapsed / kSchedulerTicksPerSecond;
    }

    uint32_t realtime_microseconds() noexcept {
        const uint64_t elapsed = g_unified_scheduler.tick_count() - g_boot_tick;
        return static_cast<uint32_t>((elapsed % kSchedulerTicksPerSecond) *
                                     (1000000U / kSchedulerTicksPerSecond));
    }

} // namespace xinim::kernel::x86_64

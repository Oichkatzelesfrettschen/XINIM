#pragma once

#include "io_port.hpp"

#include <stdint.h>

namespace xinim::i486::pit_deadline {

constexpr uint32_t kInputHz = 1193182U;

// Channel 2 supplies polling time while channel 0 retains scheduler ownership.
inline void initialize() noexcept {
    const uint8_t control = io_port::inb(0x61U);
    io_port::outb(0x61U, static_cast<uint8_t>(control & ~0x03U));
    io_port::outb(0x43U, 0xB4U);
    io_port::outb(0x42U, 0U);
    io_port::outb(0x42U, 0U);
    io_port::outb(0x61U, static_cast<uint8_t>((control & ~0x03U) | 0x01U));
}

[[nodiscard]] inline uint16_t read_counter() noexcept {
    io_port::outb(0x43U, 0x80U);
    const uint16_t low = io_port::inb(0x42U);
    const uint16_t high = io_port::inb(0x42U);
    return static_cast<uint16_t>(low | static_cast<uint16_t>(high << 8U));
}

class Deadline {
public:
    explicit Deadline(uint32_t budget_ticks) noexcept
        : previous_count_(read_counter()), remaining_ticks_(budget_ticks) {}

    [[nodiscard]] bool expired() noexcept {
        const uint16_t count = read_counter();
        const uint16_t elapsed = static_cast<uint16_t>(previous_count_ - count);
        previous_count_ = count;
        if (elapsed >= remaining_ticks_) {
            remaining_ticks_ = 0U;
            return true;
        }
        remaining_ticks_ -= elapsed;
        return false;
    }

private:
    // Poll at least once per 65536 PIT ticks (54.9 ms). Longer pauses discard
    // whole periods and conservatively extend the accumulated polling budget.
    uint16_t previous_count_;
    uint32_t remaining_ticks_;
};

} // namespace xinim::i486::pit_deadline

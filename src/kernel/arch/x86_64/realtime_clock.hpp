#pragma once

#include <cstdint>

namespace xinim::kernel::x86_64 {

    [[nodiscard]] bool initialize_realtime_clock() noexcept;
    [[nodiscard]] uint64_t realtime_seconds() noexcept;
    [[nodiscard]] uint32_t realtime_microseconds() noexcept;

} // namespace xinim::kernel::x86_64

#pragma once

#include <stdint.h>

namespace xinim::i486::user_backing {

    inline constexpr uint32_t kImageBytes = 4U * 1024U * 1024U;
    inline constexpr uint32_t kMaximumImages = 9U;

    // Reserve before device initialization; keep one candidate slot for exec.
    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] uint32_t capacity_images() noexcept;
    [[nodiscard]] uint32_t capacity_bytes() noexcept;
    [[nodiscard]] uint8_t *acquire() noexcept;
    [[nodiscard]] bool release(uint8_t *image) noexcept;
    [[nodiscard]] uint32_t live_bytes() noexcept;
    [[nodiscard]] uint32_t reserved_bytes() noexcept;

} // namespace xinim::i486::user_backing

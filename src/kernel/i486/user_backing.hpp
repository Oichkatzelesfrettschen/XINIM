#pragma once

#include <stdint.h>

namespace xinim::i486::user_backing {

    // Each slot is one user segment window: offsets 0 through kSlotBytes - 1
    // reach only this slot. The image occupies the top kImageBytes, so offsets
    // below the user virtual base land in the slot's own zeroed guard span.
    inline constexpr uint32_t kSlotBytes = 4U * 1024U * 1024U;
    inline constexpr uint32_t kImageOffsetBytes = 64U * 1024U;
    inline constexpr uint32_t kImageBytes = kSlotBytes - kImageOffsetBytes;
    inline constexpr uint32_t kMaximumImages = 9U;
    // Init, its hold service, one shell child, and an exec candidate.
    inline constexpr uint32_t kMinimumImages = 4U;

    // Reserve private segment windows before device initialization.
    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] uint32_t capacity_images() noexcept;
    [[nodiscard]] uint32_t capacity_bytes() noexcept;
    [[nodiscard]] uint8_t *acquire() noexcept;
    [[nodiscard]] bool release(uint8_t *image) noexcept;
    [[nodiscard]] uint32_t live_bytes() noexcept;
    [[nodiscard]] uint32_t reserved_bytes() noexcept;

} // namespace xinim::i486::user_backing

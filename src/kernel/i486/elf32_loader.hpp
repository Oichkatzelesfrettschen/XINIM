#pragma once

#include <stdint.h>

namespace xinim::i486::elf32 {

inline constexpr uint32_t kUserVirtualBase = 0x00400000U;
inline constexpr uint32_t kUserAddressSpaceSize = 0x00100000U;

struct UserImage {
    uint32_t entry_point;
    uint32_t stack_top;
};

bool load_static_image(const uint8_t* image,
                       uint32_t size,
                       uint8_t* address_space,
                       uint32_t address_space_size,
                       UserImage* out) noexcept;

} // namespace xinim::i486::elf32

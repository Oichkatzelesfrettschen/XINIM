#pragma once

#include <stdint.h>

namespace xinim::i486::elf32 {

// Offsets below the base stay outside the user range, so syscall pointer
// validation rejects NULL and small NULL-relative pointers with EFAULT.
inline constexpr uint32_t kUserVirtualBase = 0x00010000U;
// The image fills the rest of its 4 MiB segment window.
inline constexpr uint32_t kUserAddressSpaceSize = 0x00400000U - kUserVirtualBase;

struct UserImage {
    uint32_t entry_point;
    uint32_t brk_start;
    uint32_t stack_top;
};

bool inspect_static_image(const uint8_t* image,
                          uint32_t size,
                          UserImage* out) noexcept;

bool load_static_image(const uint8_t* image,
                       uint32_t size,
                       uint8_t* address_space,
                       uint32_t address_space_size,
                       UserImage* out) noexcept;

} // namespace xinim::i486::elf32

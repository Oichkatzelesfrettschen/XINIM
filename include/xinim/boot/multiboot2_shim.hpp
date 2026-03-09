#pragma once

#include <stdint.h>

#include "xinim/boot/bootinfo.hpp"

namespace xinim::boot {

BootInfo from_multiboot2(uint32_t magic, uintptr_t info_addr) noexcept;

} // namespace xinim::boot

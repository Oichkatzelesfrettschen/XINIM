#pragma once
#include <stdint.h>
#include <xinim/boot/bootinfo.hpp>

namespace xinim::boot {
BootInfo from_limine() noexcept;
}

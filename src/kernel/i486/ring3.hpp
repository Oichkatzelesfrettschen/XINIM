#pragma once

#include <stdint.h>

#include "xinim/boot/bootinfo.hpp"

namespace xinim::i486::ring3 {

bool launch_xash(const xinim::boot::BootInfo& info) noexcept;
[[noreturn]] void handle_fault(uint32_t vector, uint32_t error_code) noexcept;

} // namespace xinim::i486::ring3

extern "C" [[noreturn]] void i486_handle_fault(uint32_t vector,
                                                uint32_t error_code) noexcept;

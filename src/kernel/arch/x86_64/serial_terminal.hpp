#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::kernel::x86_64 {

    [[nodiscard]] int64_t serial_terminal_read(uint64_t descriptor, uintptr_t user_buffer,
                                               size_t count) noexcept;

    [[nodiscard]] int64_t serial_terminal_write(uint64_t descriptor, uintptr_t user_buffer,
                                                size_t count) noexcept;

    [[nodiscard]] int64_t serial_terminal_ioctl(uint64_t request, uintptr_t argument) noexcept;

    [[nodiscard]] bool serial_terminal_has_input() noexcept;

} // namespace xinim::kernel::x86_64

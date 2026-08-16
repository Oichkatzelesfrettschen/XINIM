#pragma once

#include <cstdint>

namespace xinim::kernel::x86_64 {

    [[nodiscard]] int64_t process_select(int descriptor_count, uintptr_t read_set,
                                         uintptr_t write_set, uintptr_t exception_set,
                                         uintptr_t timeout) noexcept;

} // namespace xinim::kernel::x86_64

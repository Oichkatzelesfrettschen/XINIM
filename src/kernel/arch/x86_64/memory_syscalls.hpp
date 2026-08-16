#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::kernel::x86_64 {

    [[nodiscard]] int64_t process_mmap(uintptr_t address, size_t length, int protection, int flags,
                                       int descriptor, uint64_t offset) noexcept;

    [[nodiscard]] int64_t process_munmap(uintptr_t address, size_t length) noexcept;

    [[nodiscard]] int64_t process_mremap(uintptr_t old_address, size_t old_length,
                                         size_t new_length, unsigned long flags,
                                         uintptr_t new_address) noexcept;

} // namespace xinim::kernel::x86_64

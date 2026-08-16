#pragma once

#include "user_address_space.hpp"

#include <cstdint>
#include <xinim/abi/exec_limits.h>

namespace xinim::kernel::x86_64 {

    inline constexpr uint64_t kInitialUserStackTop = 0x00007fffffffe000ULL;
    inline constexpr uint64_t kInitialUserStackSize = XINIM_X86_64_INITIAL_STACK_SIZE_BYTES;
    inline constexpr uint64_t kInitialUserStackBottom =
        kInitialUserStackTop - kInitialUserStackSize;

    struct Elf64UserImage {
        UserAddressSpace address_space{};
        uint64_t entry_point{0U};
        uint64_t stack_pointer{0U};
        uint64_t program_break{0U};
    };

    enum class Elf64LoadError : int {
        None = 0,
        NotFound = -1,
        InvalidImage = -2,
        UnsupportedImage = -3,
        OutOfMemory = -4,
        ArgumentsTooLarge = -5,
    };

    [[nodiscard]] Elf64LoadError
    load_bootfs_elf64_user_image(const char *pathname, Elf64UserImage &image,
                                 const char *const *arguments = nullptr,
                                 const char *const *environment = nullptr) noexcept;

} // namespace xinim::kernel::x86_64

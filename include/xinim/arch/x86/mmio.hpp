/**
 * @file mmio.hpp
 * @brief Shared x86 physical-to-MMIO mapping helpers.
 */
#pragma once

#include <cstdint>

#include <xinim/boot/bootinfo.hpp>

namespace xinim::arch::x86::mmio {

inline uintptr_t map_physical(uint64_t phys_addr) noexcept {
    if (phys_addr == 0U) {
        return 0U;
    }

#if defined(XINIM_ARCH_X86_64)
    return static_cast<uintptr_t>(phys_addr + xinim::boot::get_info().hhdm_offset);
#else
    return static_cast<uintptr_t>(phys_addr);
#endif
}

template <typename T = void>
inline T* map_pointer(uint64_t phys_addr) noexcept {
    return reinterpret_cast<T*>(map_physical(phys_addr));
}

} // namespace xinim::arch::x86::mmio

#include <cstdint>
#include <xinim/boot/bootinfo.hpp>
#include "limine_protocol.h"

namespace xinim::boot {

// Forward declare to ensure it's available
BootInfo from_limine() noexcept;

namespace limine {

// The only function now is to parse the boot info
BootInfo parse_limine_boot_info() noexcept {
    BootInfo info{};

    // Minimal parsing based on what's in limine_protocol.h
    // This is a stub, and a real implementation would need to
    // find the requests and parse them. For now, we'll just
    // return an empty BootInfo to get the build to pass.

    return info;
}

} // namespace limine

BootInfo from_limine() noexcept {
    return limine::parse_limine_boot_info();
}

} // namespace xinim::boot

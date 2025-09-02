#pragma once
#include <stdint.h>
#include <stddef.h>

namespace xinim::boot {

struct MemRange { uint64_t base{}, length{}; uint32_t type{}; };

struct BootInfo {
    const char* cmdline{nullptr};
    const void* acpi_rsdp{nullptr};
    uint64_t hhdm_offset{0};
    const MemRange* memory_map{nullptr};
    size_t memory_map_entries{0};
    const void* modules{nullptr};
    size_t modules_count{0};
};

} // namespace xinim::boot

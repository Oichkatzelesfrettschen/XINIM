#pragma once

#include <stddef.h>
#include <stdint.h>
#include <xinim/boot/bootinfo.hpp>

struct limine_memmap_response;

namespace xinim::boot {

    /**
     * Maximum Limine memory-map entries copied into bounded early-boot storage.
     *
     * Limine does not impose this limit. XINIM rejects larger maps atomically,
     * leaves BootInfo without a memory map, and then stops boot during physical
     * allocator initialization instead of truncating the firmware map.
     */
    inline constexpr size_t kLimineMemoryRangeCapacity = kBootMemoryRangeCapacity;

    [[nodiscard]] uint32_t translate_limine_memory_type(uint64_t type) noexcept;

    [[nodiscard]] bool normalize_limine_memory_map(const limine_memmap_response *response,
                                                   MemRange *output, size_t output_capacity,
                                                   size_t &output_count) noexcept;

    BootInfo from_limine() noexcept;

} // namespace xinim::boot

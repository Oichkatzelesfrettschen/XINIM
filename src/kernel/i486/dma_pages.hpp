#pragma once

#include <stdint.h>
#include "xinim/boot/bootinfo.hpp"

namespace xinim::i486::dma {

struct DmaBuffer {
    uint8_t* address;  // Physical == virtual on i486 (no paging)
    uint32_t size;
};

struct PhysicalRange {
    uint64_t base;
    uint64_t size;
};

// Keep the kernel and bootloader-owned bytes outside the DMA bump pool.
void initialize(const boot::BootInfo& info, PhysicalRange kernel,
                PhysicalRange boot_metadata) noexcept;
DmaBuffer allocate(uint32_t size, uint32_t alignment = 4096U) noexcept;
// Reserve physical space without touching it; the owner initializes bytes on use.
DmaBuffer reserve(uint32_t size, uint32_t alignment = 4096U) noexcept;
void free(const DmaBuffer& buffer) noexcept;
uint32_t available_bytes() noexcept;

} // namespace xinim::i486::dma

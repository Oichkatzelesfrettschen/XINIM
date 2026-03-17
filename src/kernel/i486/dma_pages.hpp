#pragma once

#include <stdint.h>

namespace xinim::i486::dma {

struct DmaBuffer {
    uint8_t* address;  // Physical == virtual on i486 (no paging)
    uint32_t size;
};

void initialize(const void* memory_map, uint32_t entry_count) noexcept;
DmaBuffer allocate(uint32_t size, uint32_t alignment = 4096U) noexcept;
void free(const DmaBuffer& buffer) noexcept;
uint32_t available_bytes() noexcept;

} // namespace xinim::i486::dma

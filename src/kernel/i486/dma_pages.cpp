#include "dma_pages.hpp"
#include "xinim/boot/bootinfo.hpp"

namespace xinim::i486::dma {
namespace {

// Simple bump allocator over free physical memory regions.
// On i486 without paging enabled, physical == virtual addresses.
// We reserve memory above 4MB (kernel + user space lives below).

constexpr uint32_t kMinDmaAddress = 0x00400000U; // 4 MB (above kernel)
constexpr uint32_t kMaxDmaAddress = 0x04000000U; // 64 MB (QEMU RAM limit)
constexpr uint32_t kPageSize = 4096U;

struct FreeRegion {
    uint32_t base;
    uint32_t size;
};

constexpr uint32_t kMaxRegions = 8U;
FreeRegion g_regions[kMaxRegions]{};
uint32_t g_region_count = 0U;

// Bump pointer within the first usable region
uint32_t g_bump_base = 0U;
uint32_t g_bump_end = 0U;
uint32_t g_bump_cursor = 0U;
bool g_initialized = false;

uint32_t align_up(uint32_t value, uint32_t alignment) noexcept {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

} // namespace

void initialize(const void* memory_map, uint32_t entry_count) noexcept {
    if (memory_map == nullptr || entry_count == 0U) {
        return;
    }

    const auto* ranges = static_cast<const xinim::boot::MemRange*>(memory_map);
    g_region_count = 0U;

    for (uint32_t i = 0U; i < entry_count && g_region_count < kMaxRegions; ++i) {
        // Type 1 = available RAM
        if (ranges[i].type != 1U) {
            continue;
        }

        uint64_t base64 = ranges[i].base;
        uint64_t end64 = base64 + ranges[i].length;

        // Clamp to 32-bit address space
        if (base64 >= kMaxDmaAddress) continue;
        if (end64 > kMaxDmaAddress) end64 = kMaxDmaAddress;

        uint32_t base = static_cast<uint32_t>(base64);
        uint32_t end = static_cast<uint32_t>(end64);

        // Skip regions below our minimum
        if (end <= kMinDmaAddress) continue;
        if (base < kMinDmaAddress) base = kMinDmaAddress;

        // Page-align
        base = align_up(base, kPageSize);
        end = end & ~(kPageSize - 1U);

        if (base >= end) continue;

        g_regions[g_region_count].base = base;
        g_regions[g_region_count].size = end - base;
        ++g_region_count;
    }

    // Use the largest region as our bump allocation pool
    uint32_t best = 0U;
    uint32_t best_size = 0U;
    for (uint32_t i = 0U; i < g_region_count; ++i) {
        if (g_regions[i].size > best_size) {
            best = i;
            best_size = g_regions[i].size;
        }
    }

    if (best_size > 0U) {
        g_bump_base = g_regions[best].base;
        g_bump_end = g_regions[best].base + g_regions[best].size;
        g_bump_cursor = g_bump_base;
        g_initialized = true;
    }
}

DmaBuffer allocate(uint32_t size, uint32_t alignment) noexcept {
    DmaBuffer result{nullptr, 0U};
    if (!g_initialized || size == 0U) {
        return result;
    }

    const uint32_t aligned_cursor = align_up(g_bump_cursor, alignment);
    if (aligned_cursor + size > g_bump_end || aligned_cursor < g_bump_cursor) {
        return result; // Out of DMA memory
    }

    result.address = reinterpret_cast<uint8_t*>(aligned_cursor);
    result.size = size;

    // Zero the allocated region
    for (uint32_t i = 0U; i < size; ++i) {
        result.address[i] = 0U;
    }

    g_bump_cursor = aligned_cursor + size;
    return result;
}

void free(const DmaBuffer& buffer) noexcept {
    // Bump allocator does not support free.
    // Memory is reclaimed only on full reset.
    (void)buffer;
}

uint32_t available_bytes() noexcept {
    if (!g_initialized) return 0U;
    return (g_bump_end > g_bump_cursor) ? (g_bump_end - g_bump_cursor) : 0U;
}

} // namespace xinim::i486::dma

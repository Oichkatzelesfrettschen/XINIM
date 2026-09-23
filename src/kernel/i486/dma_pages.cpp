#include "dma_pages.hpp"

namespace xinim::i486::dma {
namespace {

constexpr uint64_t kMinDmaAddress = 0x00100000U;
constexpr uint64_t kAddressSpaceEnd = uint64_t{1} << 32U;
constexpr uint64_t kPageSize = 4096U;
uint32_t g_bump_cursor = 0U;
uint32_t g_bump_end = 0U;

bool valid_range(PhysicalRange range) noexcept {
    return range.base < kAddressSpaceEnd && range.size <= kAddressSpaceEnd - range.base;
}

uint64_t page_up(uint64_t address) noexcept {
    return (address + kPageSize - 1U) & ~(kPageSize - 1U);
}

// A retained span either covers the cursor or bounds the next free gap.
void exclude_range(PhysicalRange range, uint64_t cursor,
                   uint64_t& covered_end, uint64_t& next_start) noexcept {
    if (range.size == 0U) {
        return;
    }
    const uint64_t range_end = range.base + range.size;
    if (range.base <= cursor && range_end > covered_end) {
        covered_end = range_end;
    } else if (range.base > cursor && range.base < next_start) {
        next_start = range.base;
    }
}

} // namespace

void initialize(const boot::BootInfo& info, PhysicalRange kernel,
                PhysicalRange boot_metadata) noexcept {
    g_bump_cursor = 0U;
    g_bump_end = 0U;
    if (info.memory_map == nullptr || info.memory_map_entries == 0U ||
        info.memory_map_entries > boot::kBootMemoryRangeCapacity ||
        info.modules_count > boot::kBootModuleCapacity ||
        (info.modules_count != 0U && info.modules == nullptr) ||
        kernel.size == 0U || boot_metadata.size == 0U ||
        !valid_range(kernel) || !valid_range(boot_metadata)) {
        return;
    }
    for (size_t index = 0U; index < info.memory_map_entries; ++index) {
        const auto& range = info.memory_map[index];
        if (range.length > UINT64_MAX - range.base) {
            return;
        }
    }
    for (size_t index = 0U; index < info.modules_count; ++index) {
        const auto& module = info.modules[index];
        if (!valid_range({reinterpret_cast<uintptr_t>(module.address), module.size})) {
            return;
        }
    }

    // Firmware maps describe available RAM, including the loaded ELF and modules.
    // Subtract occupied spans before choosing one contiguous DMA pool. Repeated
    // scans avoid a second array of memory-map fragments in the kernel BSS.
    uint64_t largest_size = 0U;
    for (size_t index = 0U; index < info.memory_map_entries; ++index) {
        const auto& range = info.memory_map[index];
        if (range.type != boot::MEMORY_RANGE_USABLE || range.base >= kAddressSpaceEnd) {
            continue;
        }
        uint64_t cursor = page_up(range.base < kMinDmaAddress ? kMinDmaAddress : range.base);
        uint64_t range_end = range.base + range.length;
        // Keep the exclusive pool end representable in the 32-bit bump state.
        if (range_end > UINT32_MAX) {
            range_end = UINT32_MAX;
        }
        range_end &= ~(kPageSize - 1U);
        while (cursor < range_end) {
            uint64_t covered_end = cursor;
            uint64_t next_start = range_end;
            exclude_range(kernel, cursor, covered_end, next_start);
            exclude_range(boot_metadata, cursor, covered_end, next_start);
            for (size_t module_index = 0U; module_index < info.modules_count; ++module_index) {
                const auto& module = info.modules[module_index];
                exclude_range({reinterpret_cast<uintptr_t>(module.address), module.size},
                              cursor, covered_end, next_start);
            }
            for (size_t reserved_index = 0U; reserved_index < info.memory_map_entries;
                 ++reserved_index) {
                const auto& reserved = info.memory_map[reserved_index];
                if (reserved.type != boot::MEMORY_RANGE_USABLE) {
                    exclude_range({reserved.base, reserved.length}, cursor, covered_end, next_start);
                }
            }
            if (covered_end > cursor) {
                cursor = covered_end >= range_end ? range_end : page_up(covered_end);
                continue;
            }
            const uint64_t gap_end = next_start & ~(kPageSize - 1U);
            if (gap_end > cursor && gap_end - cursor > largest_size) {
                g_bump_cursor = static_cast<uint32_t>(cursor);
                g_bump_end = static_cast<uint32_t>(gap_end);
                largest_size = gap_end - cursor;
            }
            cursor = next_start;
        }
    }
}

DmaBuffer reserve(uint32_t size, uint32_t alignment) noexcept {
    if (size == 0U || alignment == 0U || (alignment & (alignment - 1U)) != 0U ||
        g_bump_cursor == 0U) {
        return {nullptr, 0U};
    }
    const uint64_t aligned_cursor =
        (uint64_t{g_bump_cursor} + alignment - 1U) & ~(uint64_t{alignment} - 1U);
    if (aligned_cursor > g_bump_end || size > g_bump_end - aligned_cursor) {
        return {nullptr, 0U};
    }
    // The allocator returns an identity-mapped physical RAM address.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    auto *address = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(aligned_cursor));
    g_bump_cursor = static_cast<uint32_t>(aligned_cursor + size);
    return {address, size};
}

DmaBuffer allocate(uint32_t size, uint32_t alignment) noexcept {
    const DmaBuffer buffer = reserve(size, alignment);
    if (buffer.address != nullptr) {
        for (uint32_t index = 0U; index < size; ++index) {
            buffer.address[index] = 0U;
        }
    }
    return buffer;
}

void free(const DmaBuffer& buffer) noexcept {
    // Device rings retain their physical addresses until the next boot.
    (void)buffer;
}

uint32_t available_bytes() noexcept {
    return g_bump_end - g_bump_cursor;
}

} // namespace xinim::i486::dma

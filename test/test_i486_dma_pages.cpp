#include "../src/kernel/i486/dma_pages.hpp"
#include "i486_check.hpp"

#include <cstddef>
#include <cstring>
#include <sys/mman.h>

namespace {
constexpr std::size_t kPageSize = 4096U;
constexpr std::size_t kArenaSize = 32U * kPageSize;
}

int main() {
    using namespace xinim;
    int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_32BIT
    map_flags |= MAP_32BIT;
#endif
    void* mapping = mmap(nullptr, kArenaSize, PROT_READ | PROT_WRITE, map_flags, -1, 0);
    CHECK(mapping != MAP_FAILED);
    auto* bytes = static_cast<uint8_t*>(mapping);
    const uintptr_t base = reinterpret_cast<uintptr_t>(mapping);
    CHECK(base >= 0x00100000U && base <= UINT32_MAX - kArenaSize);
    std::memset(bytes, 0xA5, kArenaSize);

    // The loaded kernel occupies the start of usable RAM. Modules and retained
    // tag bytes split the remainder; the firmware reservation takes precedence
    // even when a usable descriptor overlaps it.
    boot::MemRange ranges[] = {
        {base, kArenaSize, boot::MEMORY_RANGE_USABLE},
        {base + 28U * kPageSize, 4U * kPageSize, boot::MEMORY_RANGE_RESERVED},
    };
    boot::BootModule modules[] = {
        {bytes + 16U * kPageSize + 7U, 2U * kPageSize, nullptr},
        {bytes + 18U * kPageSize, kPageSize, nullptr},
    };
    boot::BootInfo info{};
    info.memory_map = ranges;
    info.memory_map_entries = 2U;
    info.modules = modules;
    info.modules_count = 2U;
    const i486::dma::PhysicalRange kernel{base, 8U * kPageSize};
    const i486::dma::PhysicalRange metadata{base + 27U * kPageSize + 11U, kPageSize};
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 8U * kPageSize);
    const auto reserved = i486::dma::reserve(kPageSize);
    CHECK(reserved.address == bytes + 8U * kPageSize);
    CHECK(bytes[8U * kPageSize] == 0xA5U);
    const auto buffer = i486::dma::allocate(2U * kPageSize);
    CHECK(buffer.address == bytes + 9U * kPageSize);
    CHECK(buffer.size == 2U * kPageSize);
    for (uint32_t index = 0U; index < kArenaSize; ++index) {
        const uint8_t expected = index >= 9U * kPageSize && index < 11U * kPageSize ? 0U : 0xA5U;
        CHECK(bytes[index] == expected);
    }
    const uint32_t remaining = i486::dma::available_bytes();
    CHECK(i486::dma::allocate(1U, 0U).address == nullptr);
    CHECK(i486::dma::allocate(1U, 3U).address == nullptr);
    CHECK(i486::dma::allocate(0U).address == nullptr);
    CHECK(i486::dma::allocate(UINT32_MAX, 1U).address == nullptr);
    CHECK(i486::dma::allocate(1U, 0x80000000U).address == nullptr);
    CHECK(i486::dma::available_bytes() == remaining);
    CHECK(i486::dma::allocate(remaining, 1U).size == remaining);
    CHECK(i486::dma::available_bytes() == 0U);
    CHECK(i486::dma::allocate(1U).address == nullptr);

    ranges[1] = {base + 10U * kPageSize, 22U * kPageSize, boot::MEMORY_RANGE_RESERVED};
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 2U * kPageSize);
    CHECK(i486::dma::allocate(kPageSize).address == bytes + 8U * kPageSize);
    ranges[1] = {base + 28U * kPageSize, 4U * kPageSize, boot::MEMORY_RANGE_RESERVED};

    // A later usable descriptor and a module extending beyond a page boundary
    // must remain visible; pool selection cannot stop after eight descriptors.
    boot::MemRange fragmented[10]{};
    for (size_t index = 0U; index < 9U; ++index) {
        fragmented[index] = {base + 8U * kPageSize, kPageSize, boot::MEMORY_RANGE_USABLE};
    }
    fragmented[9] = {base + 19U * kPageSize, 8U * kPageSize, boot::MEMORY_RANGE_USABLE};
    info.memory_map = fragmented;
    info.memory_map_entries = 10U;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 8U * kPageSize);
    CHECK(i486::dma::allocate(1U).address == bytes + 19U * kPageSize);
    CHECK(i486::dma::allocate(1U, kPageSize).address == bytes + 20U * kPageSize);

    // Invalid initialization discards the previous pool instead of keeping a
    // stale DMA destination that a subsequent boot description has reclaimed.
    info.memory_map = nullptr;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 0U);
    CHECK(i486::dma::allocate(1U).address == nullptr);
    info.memory_map = ranges;
    info.memory_map_entries = 2U;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() != 0U);
    ranges[0].base = UINT64_MAX - 4U;
    ranges[0].length = 8U;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 0U);
    ranges[0] = {base, kArenaSize, boot::MEMORY_RANGE_USABLE};
    modules[0].size = UINT64_MAX;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 0U);
    modules[0].size = 2U * kPageSize;
    i486::dma::initialize(info, {UINT32_MAX, 2U}, metadata);
    CHECK(i486::dma::available_bytes() == 0U);
    i486::dma::initialize(info, kernel, {base, 0U});
    CHECK(i486::dma::available_bytes() == 0U);
    info.modules = nullptr;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 0U);
    info.modules = modules;
    info.modules_count = boot::kBootModuleCapacity + 1U;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 0U);
    info.modules_count = 2U;
    info.memory_map_entries = boot::kBootMemoryRangeCapacity + 1U;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 0U);

    // Clipping a usable descriptor at the 32-bit limit must avoid wrapped
    // addresses even though host-side tests cannot dereference physical RAM.
    ranges[0] = {0xFFFFC000U, 0x100000U, boot::MEMORY_RANGE_USABLE};
    info.memory_map_entries = 1U;
    info.modules_count = 0U;
    i486::dma::initialize(info, kernel, metadata);
    CHECK(i486::dma::available_bytes() == 3U * kPageSize);
    CHECK(i486::dma::allocate(1U, 0x80000000U).address == nullptr);
    CHECK(i486::dma::available_bytes() == 3U * kPageSize);

    CHECK(munmap(mapping, kArenaSize) == 0);
    std::puts("i486 DMA ownership, fragmentation, bounds, and reset checks passed");
    return 0;
}

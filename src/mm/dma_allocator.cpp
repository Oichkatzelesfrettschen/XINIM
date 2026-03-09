// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// DMA Allocator Implementation
// Provides physically contiguous memory for DMA operations

#include <xinim/mm/dma_allocator.hpp>
#include <xinim/mm/pmm.hpp>
#include <xinim/log.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace xinim::mm {

constexpr size_t PAGE_SIZE = 4096;
constexpr uint64_t DMA32_LIMIT = 0x100000000ULL;     // 4GB limit for 32-bit DMA
constexpr uint64_t DMA16_LIMIT = 0x01000000ULL;      // 16MB limit for legacy ISA DMA
constexpr uint64_t ISA_DMA_WINDOW = 0x00010000ULL;   // 64KiB 8237/ISA DMA window
constexpr uint64_t HOST_DMA16_BASE = 0x00100000ULL;  // 1MiB synthetic low-memory aperture
constexpr uint64_t HOST_DMA32_BASE = 0x02000000ULL;  // 32MiB synthetic generic DMA aperture

static bool g_initialized = false;
static size_t g_total_allocated = 0;

[[maybe_unused]] constexpr uint64_t KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;

#if __STDC_HOSTED__ == 1
struct HostedAllocation {
    void* virtual_addr;
    uint64_t physical_addr;
    size_t requested_size;
    size_t backing_size;
};

std::mutex g_host_dma_lock;
std::unordered_map<const void*, HostedAllocation> g_host_dma_allocations;
uint64_t g_host_next_dma16 = HOST_DMA16_BASE;
uint64_t g_host_next_dma32 = HOST_DMA32_BASE;
#endif

[[nodiscard]] constexpr bool has_flag(const uint32_t flags, DMAFlags flag) noexcept {
    return (flags & static_cast<uint32_t>(flag)) != 0U;
}

[[nodiscard]] constexpr bool requires_legacy_dma(const uint32_t flags) noexcept {
    return has_flag(flags, DMAFlags::BELOW_16MB);
}

[[nodiscard]] constexpr uint64_t align_up_u64(uint64_t value, uint64_t alignment) noexcept {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

[[nodiscard]] constexpr size_t align_up_size(size_t value, size_t alignment) noexcept {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

[[nodiscard]] constexpr bool crosses_boundary(uint64_t phys_addr, size_t size,
                                              uint64_t boundary) noexcept {
    if (size == 0) {
        return false;
    }

    const uint64_t last = phys_addr + static_cast<uint64_t>(size) - 1U;
    return (phys_addr / boundary) != (last / boundary);
}

[[nodiscard]] constexpr size_t effective_alignment(size_t alignment, uint32_t flag_bits) noexcept {
    size_t effective = std::max(alignment, PAGE_SIZE);
    if (requires_legacy_dma(flag_bits)) {
        effective = std::max(effective, static_cast<size_t>(ISA_DMA_WINDOW));
    }
    return effective;
}

#if __STDC_HOSTED__ == 1
[[nodiscard]] uint64_t reserve_host_physical_range(size_t requested_size, size_t alignment,
                                                   uint32_t flag_bits) {
    uint64_t& next_phys = requires_legacy_dma(flag_bits) ? g_host_next_dma16 : g_host_next_dma32;
    uint64_t candidate = align_up_u64(next_phys, static_cast<uint64_t>(alignment));

    if (requires_legacy_dma(flag_bits)) {
        if (requested_size > ISA_DMA_WINDOW) {
            return 0;
        }
        if (crosses_boundary(candidate, requested_size, ISA_DMA_WINDOW)) {
            candidate = align_up_u64(candidate, ISA_DMA_WINDOW);
        }
        if (candidate + requested_size > DMA16_LIMIT) {
            return 0;
        }
    }

    if (has_flag(flag_bits, DMAFlags::BELOW_4GB) && candidate + requested_size > DMA32_LIMIT) {
        return 0;
    }

    next_phys = candidate + align_up_u64(static_cast<uint64_t>(requested_size), PAGE_SIZE);
    return candidate;
}

[[nodiscard]] DMABuffer allocate_host_buffer(size_t size, size_t alignment, DMAFlags flags,
                                             uint32_t flag_bits) {
    const size_t pages_needed = (size + PAGE_SIZE - 1U) / PAGE_SIZE;
    const size_t requested_size = pages_needed * PAGE_SIZE;
    const size_t host_alignment = effective_alignment(alignment, flag_bits);
    const size_t backing_size = align_up_size(requested_size, host_alignment);

    void* virt_addr = std::aligned_alloc(host_alignment, backing_size);
    if (virt_addr == nullptr) {
        return DMABuffer{};
    }

    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    const uint64_t phys_addr = reserve_host_physical_range(requested_size, host_alignment, flag_bits);
    if (phys_addr == 0) {
        std::free(virt_addr);
        return DMABuffer{};
    }

    if (has_flag(flag_bits, DMAFlags::ZERO)) {
        std::memset(virt_addr, 0, backing_size);
    }

    g_host_dma_allocations.emplace(virt_addr,
                                   HostedAllocation{virt_addr, phys_addr, requested_size,
                                                    backing_size});
    g_total_allocated += backing_size;

    DMABuffer buffer;
    buffer.virtual_addr = virt_addr;
    buffer.physical_addr = phys_addr;
    buffer.size = size;
    buffer.flags = flags;
    buffer.is_coherent = has_flag(flag_bits, DMAFlags::COHERENT);
    return buffer;
}
#endif

bool DMAAllocator::initialize() {
    if (g_initialized) {
        return true;
    }

    g_total_allocated = 0;
#if __STDC_HOSTED__ == 1
    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    g_host_dma_allocations.clear();
    g_host_next_dma16 = HOST_DMA16_BASE;
    g_host_next_dma32 = HOST_DMA32_BASE;
#endif
    g_initialized = true;

    LOG_INFO("DMA: Allocator initialized (using PMM backend)");
    return true;
}

void DMAAllocator::shutdown() {
#if __STDC_HOSTED__ == 1
    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    for (const auto& [virt_base, allocation] : g_host_dma_allocations) {
        (void)virt_base;
        std::free(allocation.virtual_addr);
    }
    g_host_dma_allocations.clear();
    g_host_next_dma16 = HOST_DMA16_BASE;
    g_host_next_dma32 = HOST_DMA32_BASE;
#endif
    g_total_allocated = 0;
    g_initialized = false;
    LOG_INFO("DMA: Allocator shutdown");
}

DMABuffer DMAAllocator::allocate(size_t size, DMAFlags flags) {
    if (!g_initialized || size == 0) {
        return DMABuffer{};
    }

    const uint32_t flag_bits = static_cast<uint32_t>(flags);
    if (requires_legacy_dma(flag_bits) && size > ISA_DMA_WINDOW) {
        LOG_ERROR("DMA: requested legacy DMA buffer %zu exceeds a single 64KiB ISA DMA window", size);
        return DMABuffer{};
    }

    return allocate_aligned(size, PAGE_SIZE, flags);
}

void DMAAllocator::free(const DMABuffer& buffer) {
    if (!buffer.is_valid()) {
        return;
    }

#if __STDC_HOSTED__ == 1
    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    const auto it = g_host_dma_allocations.find(buffer.virtual_addr);
    if (it == g_host_dma_allocations.end()) {
        return;
    }

    const size_t backing_size = it->second.backing_size;
    std::free(it->second.virtual_addr);
    g_host_dma_allocations.erase(it);
    g_total_allocated -= backing_size;
#else
    const size_t pages_needed = (buffer.size + PAGE_SIZE - 1U) / PAGE_SIZE;
    const size_t alloc_size = pages_needed * PAGE_SIZE;

    auto& pmm = PhysicalMemoryManager::instance();
    pmm.free_pages(buffer.physical_addr, pages_needed);
    g_total_allocated -= alloc_size;
#endif
}

DMABuffer DMAAllocator::allocate_aligned(size_t size, size_t alignment, DMAFlags flags) {
    if (!g_initialized || size == 0) {
        return DMABuffer{};
    }

    const uint32_t flag_bits = static_cast<uint32_t>(flags);
    const size_t pages_needed = (size + PAGE_SIZE - 1U) / PAGE_SIZE;
    const size_t alloc_size = pages_needed * PAGE_SIZE;
    alignment = effective_alignment(alignment, flag_bits);

    if (requires_legacy_dma(flag_bits) && alloc_size > ISA_DMA_WINDOW) {
        LOG_ERROR("DMA: aligned legacy DMA buffer %zu exceeds a single 64KiB ISA DMA window", alloc_size);
        return DMABuffer{};
    }

#if __STDC_HOSTED__ == 1
    return allocate_host_buffer(size, alignment, flags, flag_bits);
#else
    MemoryZone zone = MemoryZone::NORMAL;
    if (has_flag(flag_bits, DMAFlags::BELOW_4GB) || requires_legacy_dma(flag_bits)) {
        zone = MemoryZone::DMA;
    }

    auto& pmm = PhysicalMemoryManager::instance();
    const uint64_t phys_addr = pmm.alloc_pages_aligned(pages_needed, alignment, zone);
    if (phys_addr == 0) {
        LOG_ERROR("DMA: Failed to allocate %zu aligned pages", pages_needed);
        return DMABuffer{};
    }

    if (has_flag(flag_bits, DMAFlags::BELOW_4GB) && phys_addr + alloc_size > DMA32_LIMIT) {
        LOG_ERROR("DMA: Aligned allocation 0x%lx exceeds 4GB limit", phys_addr);
        pmm.free_pages(phys_addr, pages_needed);
        return DMABuffer{};
    }

    if (requires_legacy_dma(flag_bits)) {
        if (phys_addr + alloc_size > DMA16_LIMIT) {
            LOG_ERROR("DMA: Aligned allocation 0x%lx exceeds 16MB limit", phys_addr);
            pmm.free_pages(phys_addr, pages_needed);
            return DMABuffer{};
        }
        if (crosses_boundary(phys_addr, alloc_size, ISA_DMA_WINDOW)) {
            LOG_ERROR("DMA: Aligned allocation 0x%lx crosses a 64KiB ISA DMA boundary", phys_addr);
            pmm.free_pages(phys_addr, pages_needed);
            return DMABuffer{};
        }
    }

    void* virt_addr = phys_to_virt(phys_addr);
    if (virt_addr == nullptr) {
        LOG_ERROR("DMA: Failed to map aligned physical address 0x%lx", phys_addr);
        pmm.free_pages(phys_addr, pages_needed);
        return DMABuffer{};
    }

    if (has_flag(flag_bits, DMAFlags::ZERO)) {
        std::memset(virt_addr, 0, alloc_size);
    }
    if (has_flag(flag_bits, DMAFlags::COHERENT)) {
        flush_cache(virt_addr, alloc_size);
    }

    g_total_allocated += alloc_size;

    DMABuffer buffer;
    buffer.virtual_addr = virt_addr;
    buffer.physical_addr = phys_addr;
    buffer.size = size;
    buffer.flags = flags;
    buffer.is_coherent = has_flag(flag_bits, DMAFlags::COHERENT);
    return buffer;
#endif
}

void* DMAAllocator::phys_to_virt(uint64_t phys_addr) {
#if __STDC_HOSTED__ == 1
    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    for (const auto& [virt_base, allocation] : g_host_dma_allocations) {
        (void)virt_base;
        const uint64_t start = allocation.physical_addr;
        const uint64_t end = start + allocation.requested_size;
        if (phys_addr >= start && phys_addr < end) {
            const size_t offset = static_cast<size_t>(phys_addr - start);
            return static_cast<unsigned char*>(allocation.virtual_addr) + offset;
        }
    }
    return nullptr;
#else
    return reinterpret_cast<void*>(phys_addr + KERNEL_VIRTUAL_BASE);
#endif
}

uint64_t DMAAllocator::virt_to_phys(const void* virt_addr) {
#if __STDC_HOSTED__ == 1
    if (virt_addr == nullptr) {
        return 0;
    }

    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    const auto* address = static_cast<const unsigned char*>(virt_addr);
    for (const auto& [virt_base, allocation] : g_host_dma_allocations) {
        const auto* base = static_cast<const unsigned char*>(virt_base);
        const auto* end = base + allocation.requested_size;
        if (address >= base && address < end) {
            const uint64_t offset = static_cast<uint64_t>(address - base);
            return allocation.physical_addr + offset;
        }
    }
    return 0;
#else
    const uint64_t virt = reinterpret_cast<uint64_t>(virt_addr);
    if (virt >= KERNEL_VIRTUAL_BASE) {
        return virt - KERNEL_VIRTUAL_BASE;
    }
    return 0;
#endif
}

void DMAAllocator::flush_cache(const void* virt_addr, size_t size) {
    constexpr size_t CACHE_LINE_SIZE = 64;

    uintptr_t addr = reinterpret_cast<uintptr_t>(virt_addr);
    uintptr_t end = addr + size;
    addr &= ~(CACHE_LINE_SIZE - 1U);

    for (; addr < end; addr += CACHE_LINE_SIZE) {
        asm volatile("clflush (%0)" :: "r"(addr) : "memory");
    }

    asm volatile("mfence" ::: "memory");
}

void DMAAllocator::invalidate_cache(const void* virt_addr, size_t size) {
    flush_cache(virt_addr, size);
}

void DMAAllocator::sync_for_device(const DMABuffer& buffer) {
    if (!buffer.is_coherent) {
        flush_cache(buffer.virtual_addr, buffer.size);
    }
}

void DMAAllocator::sync_for_cpu(const DMABuffer& buffer) {
    if (!buffer.is_coherent) {
        invalidate_cache(buffer.virtual_addr, buffer.size);
    }
}

size_t DMAAllocator::get_total_allocated() {
    return g_total_allocated;
}

size_t DMAAllocator::get_available_memory() {
#if __STDC_HOSTED__ == 1
    std::lock_guard<std::mutex> guard(g_host_dma_lock);
    return (DMA32_LIMIT - HOST_DMA32_BASE) - g_total_allocated;
#else
    auto& pmm = PhysicalMemoryManager::instance();
    return pmm.get_free_memory();
#endif
}

bool DMAAllocator::is_address_dma_capable(uint64_t phys_addr) {
#if __STDC_HOSTED__ == 1
    return phys_addr != 0 && phys_addr < DMA32_LIMIT;
#else
    if (phys_addr >= DMA32_LIMIT) {
        return false;
    }

    auto& pmm = PhysicalMemoryManager::instance();
    const MemoryZone zone = pmm.get_zone_for_address(phys_addr);
    return zone == MemoryZone::DMA || zone == MemoryZone::NORMAL;
#endif
}

} // namespace xinim::mm

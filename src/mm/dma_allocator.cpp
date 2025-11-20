// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// DMA Allocator Implementation
// Provides physically contiguous memory for DMA operations

#include <xinim/mm/dma_allocator.hpp>
#include <xinim/mm/pmm.hpp>
#include <xinim/log.hpp>
#include <cstring>

namespace xinim::mm {

// Constants
constexpr size_t PAGE_SIZE = 4096;
constexpr uint64_t DMA32_LIMIT = 0x100000000ULL;     // 4GB limit for 32-bit DMA

// Global allocator state
static bool g_initialized = false;
static size_t g_total_allocated = 0;

// Virtual memory offset for direct mapping
// Assumes kernel has identity mapping or offset mapping
constexpr uint64_t KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;

bool DMAAllocator::initialize() {
    if (g_initialized) {
        return true;
    }

    // DMA allocator now uses PhysicalMemoryManager backend
    // PMM must be initialized before DMA allocator
    g_total_allocated = 0;
    g_initialized = true;

    LOG_INFO("DMA: Allocator initialized (using PMM backend)");
    return true;
}

void DMAAllocator::shutdown() {
    g_initialized = false;
    LOG_INFO("DMA: Allocator shutdown");
}

DMABuffer DMAAllocator::allocate(size_t size, DMAFlags flags) {
    if (!g_initialized) {
        return DMABuffer{};
    }

    if (size == 0) {
        return DMABuffer{};
    }

    // Round up to page size
    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t alloc_size = pages_needed * PAGE_SIZE;

    // Determine memory zone based on flags
    MemoryZone zone = MemoryZone::NORMAL;
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::BELOW_4GB)) {
        // Allocate from DMA zone (below 16MB) or NORMAL zone (below 896MB)
        // Both are below 4GB on most systems
        zone = MemoryZone::DMA;
    }

    // Allocate physical pages from PMM
    auto& pmm = PhysicalMemoryManager::instance();
    uint64_t phys_addr = pmm.alloc_pages(pages_needed, zone);
    if (phys_addr == 0) {
        LOG_ERROR("DMA: Failed to allocate %zu pages from zone %d",
                  pages_needed, static_cast<int>(zone));
        return DMABuffer{};  // Out of memory
    }

    // Verify 32-bit DMA constraint
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::BELOW_4GB)) {
        if (phys_addr + alloc_size > DMA32_LIMIT) {
            LOG_ERROR("DMA: Allocated memory 0x%lx exceeds 4GB limit", phys_addr);
            pmm.free_pages(phys_addr, pages_needed);
            return DMABuffer{};  // Cannot satisfy 4GB constraint
        }
    }

    // Get virtual address
    void* virt_addr = phys_to_virt(phys_addr);
    if (!virt_addr) {
        LOG_ERROR("DMA: Failed to map physical address 0x%lx", phys_addr);
        pmm.free_pages(phys_addr, pages_needed);
        return DMABuffer{};
    }

    // Zero memory if requested
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::ZERO)) {
        memset(virt_addr, 0, alloc_size);
    }

    // Flush cache for coherency
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::COHERENT)) {
        flush_cache(virt_addr, alloc_size);
    }

    g_total_allocated += alloc_size;

    DMABuffer buffer;
    buffer.virtual_addr = virt_addr;
    buffer.physical_addr = phys_addr;
    buffer.size = size;
    buffer.flags = flags;
    buffer.is_coherent = static_cast<bool>(
        static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::COHERENT)
    );

    return buffer;
}

void DMAAllocator::free(const DMABuffer& buffer) {
    if (!buffer.is_valid()) {
        return;
    }

    size_t pages_needed = (buffer.size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t alloc_size = pages_needed * PAGE_SIZE;

    // Free physical pages via PMM
    auto& pmm = PhysicalMemoryManager::instance();
    pmm.free_pages(buffer.physical_addr, pages_needed);

    g_total_allocated -= alloc_size;
}

DMABuffer DMAAllocator::allocate_aligned(size_t size, size_t alignment,
                                         DMAFlags flags) {
    if (!g_initialized) {
        return DMABuffer{};
    }

    // Ensure alignment is power of 2 and >= PAGE_SIZE
    if (alignment < PAGE_SIZE) {
        alignment = PAGE_SIZE;
    }

    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t alloc_size = pages_needed * PAGE_SIZE;

    // Determine memory zone based on flags
    MemoryZone zone = MemoryZone::NORMAL;
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::BELOW_4GB)) {
        zone = MemoryZone::DMA;
    }

    // Use PMM's aligned allocation
    auto& pmm = PhysicalMemoryManager::instance();
    uint64_t phys_addr = pmm.alloc_pages_aligned(pages_needed, alignment, zone);
    if (phys_addr == 0) {
        LOG_ERROR("DMA: Failed to allocate %zu aligned pages", pages_needed);
        return DMABuffer{};
    }

    // Verify 32-bit DMA constraint
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::BELOW_4GB)) {
        if (phys_addr + alloc_size > DMA32_LIMIT) {
            LOG_ERROR("DMA: Aligned allocation 0x%lx exceeds 4GB limit", phys_addr);
            pmm.free_pages(phys_addr, pages_needed);
            return DMABuffer{};
        }
    }

    // Get virtual address
    void* virt_addr = phys_to_virt(phys_addr);
    if (!virt_addr) {
        LOG_ERROR("DMA: Failed to map aligned physical address 0x%lx", phys_addr);
        pmm.free_pages(phys_addr, pages_needed);
        return DMABuffer{};
    }

    // Zero memory if requested
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::ZERO)) {
        memset(virt_addr, 0, alloc_size);
    }

    // Flush cache for coherency
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::COHERENT)) {
        flush_cache(virt_addr, alloc_size);
    }

    g_total_allocated += alloc_size;

    DMABuffer buffer;
    buffer.virtual_addr = virt_addr;
    buffer.physical_addr = phys_addr;
    buffer.size = size;
    buffer.flags = flags;
    buffer.is_coherent = static_cast<bool>(
        static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::COHERENT)
    );

    return buffer;
}

void* DMAAllocator::phys_to_virt(uint64_t phys_addr) {
    // Direct mapping: physical address + kernel base
    // TODO: Use proper MMU translation when available
    return reinterpret_cast<void*>(phys_addr + KERNEL_VIRTUAL_BASE);
}

uint64_t DMAAllocator::virt_to_phys(const void* virt_addr) {
    // Reverse of phys_to_virt
    // TODO: Use proper MMU translation when available
    uint64_t virt = reinterpret_cast<uint64_t>(virt_addr);
    if (virt >= KERNEL_VIRTUAL_BASE) {
        return virt - KERNEL_VIRTUAL_BASE;
    }
    return 0;  // Invalid address
}

void DMAAllocator::flush_cache(const void* virt_addr, size_t size) {
    // x86_64: Use CLFLUSH instruction to flush cache lines
    // Cache line size is typically 64 bytes
    constexpr size_t CACHE_LINE_SIZE = 64;

    uintptr_t addr = reinterpret_cast<uintptr_t>(virt_addr);
    uintptr_t end = addr + size;

    // Align to cache line boundary
    addr &= ~(CACHE_LINE_SIZE - 1);

    for (; addr < end; addr += CACHE_LINE_SIZE) {
        // CLFLUSH - flush cache line
        asm volatile("clflush (%0)" :: "r"(addr) : "memory");
    }

    // Memory barrier to ensure flush completes
    asm volatile("mfence" ::: "memory");
}

void DMAAllocator::invalidate_cache(const void* virt_addr, size_t size) {
    // x86_64: Use CLFLUSHOPT or CLWB if available, otherwise CLFLUSH
    // For now, use CLFLUSH (available on all x86_64)
    flush_cache(virt_addr, size);
}

void DMAAllocator::sync_for_device(const DMABuffer& buffer) {
    // Flush CPU cache so device sees latest data
    if (!buffer.is_coherent) {
        flush_cache(buffer.virtual_addr, buffer.size);
    }
}

void DMAAllocator::sync_for_cpu(const DMABuffer& buffer) {
    // Invalidate CPU cache so CPU sees device's data
    if (!buffer.is_coherent) {
        invalidate_cache(buffer.virtual_addr, buffer.size);
    }
}

size_t DMAAllocator::get_total_allocated() {
    return g_total_allocated;
}

size_t DMAAllocator::get_available_memory() {
    auto& pmm = PhysicalMemoryManager::instance();
    return pmm.get_free_memory();
}

bool DMAAllocator::is_address_dma_capable(uint64_t phys_addr) {
    // Check if address is below 4GB and in valid memory
    if (phys_addr >= DMA32_LIMIT) {
        return false;
    }

    auto& pmm = PhysicalMemoryManager::instance();
    MemoryZone zone = pmm.get_zone_for_address(phys_addr);

    // DMA and NORMAL zones are suitable for DMA operations
    return (zone == MemoryZone::DMA || zone == MemoryZone::NORMAL);
}

} // namespace xinim::mm

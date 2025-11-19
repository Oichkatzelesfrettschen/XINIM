// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// DMA Allocator Implementation
// Provides physically contiguous memory for DMA operations

#include <xinim/mm/dma_allocator.hpp>
#include <xinim/kernel/kassert.hpp>
#include <cstring>

namespace xinim::mm {

// Constants
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t DMA_ZONE_SIZE = 16 * 1024 * 1024;  // 16MB DMA zone
constexpr uint64_t DMA32_LIMIT = 0x100000000ULL;     // 4GB limit for 32-bit DMA

// Simple physical memory allocator for DMA
// TODO: Integrate with full physical memory manager
struct PhysicalPageAllocator {
    uint64_t base_addr;
    size_t total_pages;
    size_t used_pages;
    uint8_t* bitmap;  // Bit per page

    static constexpr size_t MAX_PAGES = DMA_ZONE_SIZE / PAGE_SIZE;
    uint8_t bitmap_storage[MAX_PAGES / 8];

    PhysicalPageAllocator()
        : base_addr(0x1000000)  // Start at 16MB
        , total_pages(MAX_PAGES)
        , used_pages(0)
        , bitmap(bitmap_storage) {
        memset(bitmap, 0, sizeof(bitmap_storage));
    }

    uint64_t allocate_pages(size_t count) {
        // Find contiguous free pages
        for (size_t i = 0; i <= total_pages - count; ++i) {
            bool found = true;
            for (size_t j = 0; j < count; ++j) {
                if (is_page_used(i + j)) {
                    found = false;
                    i += j;  // Skip ahead
                    break;
                }
            }

            if (found) {
                // Mark pages as used
                for (size_t j = 0; j < count; ++j) {
                    set_page_used(i + j);
                }
                used_pages += count;
                return base_addr + (i * PAGE_SIZE);
            }
        }
        return 0;  // Out of memory
    }

    void free_pages(uint64_t phys_addr, size_t count) {
        size_t page_index = (phys_addr - base_addr) / PAGE_SIZE;
        for (size_t i = 0; i < count; ++i) {
            clear_page_used(page_index + i);
        }
        used_pages -= count;
    }

    bool is_page_used(size_t index) const {
        return bitmap[index / 8] & (1 << (index % 8));
    }

    void set_page_used(size_t index) {
        bitmap[index / 8] |= (1 << (index % 8));
    }

    void clear_page_used(size_t index) {
        bitmap[index / 8] &= ~(1 << (index % 8));
    }
};

// Global allocator state
static PhysicalPageAllocator g_phys_allocator;
static bool g_initialized = false;
static size_t g_total_allocated = 0;

// Virtual memory offset for direct mapping
// Assumes kernel has identity mapping or offset mapping
constexpr uint64_t KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;

bool DMAAllocator::initialize() {
    if (g_initialized) {
        return true;
    }

    // Initialize physical allocator
    g_phys_allocator = PhysicalPageAllocator();
    g_total_allocated = 0;
    g_initialized = true;

    return true;
}

void DMAAllocator::shutdown() {
    g_initialized = false;
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

    // Allocate physical pages
    uint64_t phys_addr = g_phys_allocator.allocate_pages(pages_needed);
    if (phys_addr == 0) {
        return DMABuffer{};  // Out of memory
    }

    // Check 32-bit DMA constraint
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DMAFlags::BELOW_4GB)) {
        if (phys_addr + alloc_size > DMA32_LIMIT) {
            g_phys_allocator.free_pages(phys_addr, pages_needed);
            return DMABuffer{};  // Cannot satisfy 4GB constraint
        }
    }

    // Get virtual address
    void* virt_addr = phys_to_virt(phys_addr);
    if (!virt_addr) {
        g_phys_allocator.free_pages(phys_addr, pages_needed);
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
    g_phys_allocator.free_pages(buffer.physical_addr, pages_needed);

    size_t alloc_size = pages_needed * PAGE_SIZE;
    g_total_allocated -= alloc_size;
}

DMABuffer DMAAllocator::allocate_aligned(size_t size, size_t alignment,
                                         DMAFlags flags) {
    // For simplicity, ensure alignment is power of 2 and >= PAGE_SIZE
    if (alignment < PAGE_SIZE) {
        alignment = PAGE_SIZE;
    }

    // Allocate extra memory to guarantee alignment
    size_t extra = alignment - PAGE_SIZE;
    size_t alloc_size = size + extra;

    DMABuffer buffer = allocate(alloc_size, flags);
    if (!buffer.is_valid()) {
        return buffer;
    }

    // Adjust addresses to meet alignment
    uint64_t aligned_phys = (buffer.physical_addr + alignment - 1) & ~(alignment - 1);
    uint64_t offset = aligned_phys - buffer.physical_addr;

    buffer.physical_addr = aligned_phys;
    buffer.virtual_addr = static_cast<char*>(buffer.virtual_addr) + offset;
    buffer.size = size;

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
    return (g_phys_allocator.total_pages - g_phys_allocator.used_pages) * PAGE_SIZE;
}

bool DMAAllocator::is_address_dma_capable(uint64_t phys_addr) {
    // Check if address is within DMA zone and below 4GB
    return (phys_addr >= g_phys_allocator.base_addr) &&
           (phys_addr < g_phys_allocator.base_addr + DMA_ZONE_SIZE) &&
           (phys_addr < DMA32_LIMIT);
}

} // namespace xinim::mm

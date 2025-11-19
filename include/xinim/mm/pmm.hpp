/**
 * @file pmm.hpp
 * @brief Physical Memory Manager - Modern Implementation
 * @author XINIM Project
 * @version 2.0
 * @date 2025
 *
 * Features:
 * - Zone-based allocation (DMA, NORMAL, HIGH)
 * - Buddy allocator for efficient multi-page allocation
 * - Page frame metadata tracking
 * - Memory statistics and debugging
 * - Thread-safe operations
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>

namespace xinim::mm {

/**
 * @brief Memory Zones
 * Based on Linux memory zones for compatibility
 */
enum class MemoryZone {
    DMA,        // 0-16MB (ISA DMA compatible)
    NORMAL,     // 16MB-896MB (directly mapped kernel memory)
    HIGH,       // >896MB (requires page table mapping)
};

/**
 * @brief Page Frame Metadata
 */
struct PageFrame {
    uint32_t flags;          // Page flags (allocated, reserved, etc.)
    uint32_t ref_count;      // Reference count
    uint32_t order;          // Buddy allocator order (0-10)
    void*    private_data;   // Driver-specific data

    // Page flags
    static constexpr uint32_t FLAG_FREE      = 0x0000;
    static constexpr uint32_t FLAG_ALLOCATED = 0x0001;
    static constexpr uint32_t FLAG_RESERVED  = 0x0002;
    static constexpr uint32_t FLAG_DMA       = 0x0004;
    static constexpr uint32_t FLAG_LOCKED    = 0x0008;
    static constexpr uint32_t FLAG_SLAB      = 0x0010;
};

/**
 * @brief Free Area for Buddy Allocator
 */
struct FreeArea {
    std::vector<uint64_t> free_list;  // List of free block physical addresses
    uint32_t nr_free;                  // Number of free blocks
};

/**
 * @brief Memory Zone Descriptor
 */
struct ZoneDescriptor {
    MemoryZone type;
    uint64_t start_pfn;          // Start page frame number
    uint64_t end_pfn;            // End page frame number
    uint64_t pages_total;        // Total pages in zone
    uint64_t pages_free;         // Free pages in zone
    uint64_t pages_reserved;     // Reserved pages in zone

    // Buddy allocator free lists (order 0-10)
    static constexpr size_t MAX_ORDER = 11;
    FreeArea free_area[MAX_ORDER];

    // Statistics
    uint64_t alloc_count;
    uint64_t free_count;

    std::mutex lock;
};

/**
 * @brief Physical Memory Manager
 */
class PhysicalMemoryManager {
public:
    static PhysicalMemoryManager& instance();

    // Initialization
    bool initialize(uint64_t total_memory, uint64_t kernel_start, uint64_t kernel_end);
    void add_memory_region(uint64_t base, uint64_t size, bool reserved = false);

    // Page Allocation
    uint64_t alloc_page(MemoryZone zone = MemoryZone::NORMAL);
    uint64_t alloc_pages(size_t count, MemoryZone zone = MemoryZone::NORMAL);
    uint64_t alloc_pages_aligned(size_t count, size_t alignment, MemoryZone zone = MemoryZone::NORMAL);

    // Page Freeing
    void free_page(uint64_t phys_addr);
    void free_pages(uint64_t phys_addr, size_t count);

    // Buddy Allocator
    uint64_t buddy_alloc(uint32_t order, MemoryZone zone = MemoryZone::NORMAL);
    void buddy_free(uint64_t phys_addr, uint32_t order);

    // Page Frame Management
    PageFrame* get_page_frame(uint64_t phys_addr);
    void ref_page(uint64_t phys_addr);
    void unref_page(uint64_t phys_addr);

    // Memory Zones
    ZoneDescriptor* get_zone(MemoryZone zone);
    MemoryZone get_zone_for_address(uint64_t phys_addr);

    // Statistics
    uint64_t get_total_memory() const { return total_memory_; }
    uint64_t get_free_memory() const;
    uint64_t get_used_memory() const;
    uint64_t get_reserved_memory() const;

    // Debugging
    void dump_zones();
    void dump_page_frame(uint64_t phys_addr);
    void dump_free_lists();

private:
    PhysicalMemoryManager();
    ~PhysicalMemoryManager() = default;

    // No copy
    PhysicalMemoryManager(const PhysicalMemoryManager&) = delete;
    PhysicalMemoryManager& operator=(const PhysicalMemoryManager&) = delete;

    // Helper functions
    uint64_t pfn_to_phys(uint64_t pfn) const { return pfn * PAGE_SIZE; }
    uint64_t phys_to_pfn(uint64_t phys) const { return phys / PAGE_SIZE; }

    uint32_t get_order(size_t pages);
    size_t get_pages_for_order(uint32_t order) const { return 1ULL << order; }

    void init_zones();
    void init_buddy_allocator();
    void add_to_free_list(uint64_t phys_addr, uint32_t order, MemoryZone zone);
    void remove_from_free_list(uint64_t phys_addr, uint32_t order, MemoryZone zone);
    uint64_t get_buddy_address(uint64_t phys_addr, uint32_t order);

    // Memory layout
    static constexpr uint64_t PAGE_SIZE = 4096;
    static constexpr uint64_t DMA_ZONE_END = 16 * 1024 * 1024;       // 16MB
    static constexpr uint64_t NORMAL_ZONE_END = 896 * 1024 * 1024;   // 896MB

    // Total memory
    uint64_t total_memory_;
    uint64_t kernel_start_;
    uint64_t kernel_end_;

    // Memory zones
    static constexpr size_t NUM_ZONES = 3;
    ZoneDescriptor zones_[NUM_ZONES];

    // Page frames metadata
    PageFrame* page_frames_;
    uint64_t total_page_frames_;

    // Global statistics
    uint64_t total_pages_;
    uint64_t free_pages_;
    uint64_t used_pages_;
    uint64_t reserved_pages_;

    std::mutex global_lock_;
};

// Helper functions
inline uint64_t pages_to_bytes(uint64_t pages) {
    return pages * 4096;
}

inline uint64_t bytes_to_pages(uint64_t bytes) {
    return (bytes + 4095) / 4096;
}

inline uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

inline uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

} // namespace xinim::mm

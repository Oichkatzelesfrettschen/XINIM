/**
 * @file meminfo.hpp
 * @brief Memory Information and Debugging Utilities
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#pragma once

#include <cstdint>
#include <string>

namespace xinim::mm {

/**
 * @brief Memory Statistics
 */
struct MemoryStats {
    uint64_t total_memory;      // Total physical memory
    uint64_t free_memory;       // Free physical memory
    uint64_t used_memory;       // Used physical memory
    uint64_t reserved_memory;   // Reserved memory (kernel, etc.)
    uint64_t cached_memory;     // Cached memory (future)
    uint64_t dma_allocated;     // DMA buffer allocations

    // Zone statistics
    uint64_t dma_zone_total;
    uint64_t dma_zone_free;
    uint64_t normal_zone_total;
    uint64_t normal_zone_free;
    uint64_t high_zone_total;
    uint64_t high_zone_free;

    // Allocation statistics
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t allocation_failures;

    // Fragmentation info
    uint64_t largest_free_block;  // In bytes
    float fragmentation_ratio;     // 0.0 = no fragmentation, 1.0 = high
};

/**
 * @brief Memory Information Utility
 */
class MemInfo {
public:
    /**
     * @brief Get current memory statistics
     */
    static MemoryStats get_stats();

    /**
     * @brief Print memory statistics to console
     */
    static void print_stats();

    /**
     * @brief Print detailed zone information
     */
    static void print_zones();

    /**
     * @brief Print free list information
     */
    static void print_free_lists();

    /**
     * @brief Print page frame information
     */
    static void print_page_info(uint64_t phys_addr);

    /**
     * @brief Print DMA allocator statistics
     */
    static void print_dma_stats();

    /**
     * @brief Format bytes to human-readable string
     */
    static std::string format_bytes(uint64_t bytes);

    /**
     * @brief Calculate memory fragmentation
     */
    static float calculate_fragmentation();
};

} // namespace xinim::mm

/**
 * @file meminfo.cpp
 * @brief Memory Information and Debugging Utilities
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/mm/meminfo.hpp>
#include <xinim/mm/pmm.hpp>
#include <xinim/mm/dma_allocator.hpp>
#include <xinim/log.hpp>
#include <cstdio>

namespace xinim::mm {

MemoryStats MemInfo::get_stats() {
    MemoryStats stats = {};

    auto& pmm = PhysicalMemoryManager::instance();

    // Overall stats
    stats.total_memory = pmm.get_total_memory();
    stats.free_memory = pmm.get_free_memory();
    stats.used_memory = pmm.get_used_memory();
    stats.reserved_memory = pmm.get_reserved_memory();
    stats.cached_memory = 0;  // Not implemented yet
    stats.dma_allocated = DMAAllocator::get_total_allocated();

    // Zone stats
    auto* dma_zone = pmm.get_zone(MemoryZone::DMA);
    if (dma_zone) {
        stats.dma_zone_total = pages_to_bytes(dma_zone->pages_total);
        stats.dma_zone_free = pages_to_bytes(dma_zone->pages_free);
        stats.total_allocations += dma_zone->alloc_count;
        stats.total_frees += dma_zone->free_count;
    }

    auto* normal_zone = pmm.get_zone(MemoryZone::NORMAL);
    if (normal_zone) {
        stats.normal_zone_total = pages_to_bytes(normal_zone->pages_total);
        stats.normal_zone_free = pages_to_bytes(normal_zone->pages_free);
        stats.total_allocations += normal_zone->alloc_count;
        stats.total_frees += normal_zone->free_count;
    }

    auto* high_zone = pmm.get_zone(MemoryZone::HIGH);
    if (high_zone) {
        stats.high_zone_total = pages_to_bytes(high_zone->pages_total);
        stats.high_zone_free = pages_to_bytes(high_zone->pages_free);
        stats.total_allocations += high_zone->alloc_count;
        stats.total_frees += high_zone->free_count;
    }

    // Calculate largest free block
    stats.largest_free_block = 0;
    for (int zone_idx = 0; zone_idx < 3; zone_idx++) {
        MemoryZone zone = static_cast<MemoryZone>(zone_idx);
        auto* zone_desc = pmm.get_zone(zone);
        if (!zone_desc) continue;

        // Find largest order with free blocks
        for (int order = ZoneDescriptor::MAX_ORDER - 1; order >= 0; order--) {
            if (zone_desc->free_area[order].nr_free > 0) {
                uint64_t block_size = (1ULL << order) * 4096;
                if (block_size > stats.largest_free_block) {
                    stats.largest_free_block = block_size;
                }
                break;
            }
        }
    }

    // Calculate fragmentation
    stats.fragmentation_ratio = calculate_fragmentation();

    return stats;
}

void MemInfo::print_stats() {
    MemoryStats stats = get_stats();

    printf("\n");
    printf("=== XINIM Memory Statistics ===\n");
    printf("\n");

    // Overall memory
    printf("Physical Memory:\n");
    printf("  Total:       %s\n", format_bytes(stats.total_memory).c_str());
    printf("  Free:        %s\n", format_bytes(stats.free_memory).c_str());
    printf("  Used:        %s\n", format_bytes(stats.used_memory).c_str());
    printf("  Reserved:    %s\n", format_bytes(stats.reserved_memory).c_str());
    printf("\n");

    // Memory zones
    printf("Memory Zones:\n");
    printf("  DMA Zone (0-16MB):\n");
    printf("    Total:     %s\n", format_bytes(stats.dma_zone_total).c_str());
    printf("    Free:      %s\n", format_bytes(stats.dma_zone_free).c_str());
    printf("\n");
    printf("  NORMAL Zone (16MB-896MB):\n");
    printf("    Total:     %s\n", format_bytes(stats.normal_zone_total).c_str());
    printf("    Free:      %s\n", format_bytes(stats.normal_zone_free).c_str());
    printf("\n");
    if (stats.high_zone_total > 0) {
        printf("  HIGH Zone (>896MB):\n");
        printf("    Total:     %s\n", format_bytes(stats.high_zone_total).c_str());
        printf("    Free:      %s\n", format_bytes(stats.high_zone_free).c_str());
        printf("\n");
    }

    // Allocation statistics
    printf("Allocation Statistics:\n");
    printf("  Total allocations: %lu\n", stats.total_allocations);
    printf("  Total frees:       %lu\n", stats.total_frees);
    printf("  Active allocs:     %lu\n",
           stats.total_allocations > stats.total_frees ?
           stats.total_allocations - stats.total_frees : 0);
    printf("\n");

    // Fragmentation
    printf("Fragmentation:\n");
    printf("  Largest free block: %s\n", format_bytes(stats.largest_free_block).c_str());
    printf("  Fragmentation:      %.1f%%\n", stats.fragmentation_ratio * 100.0f);
    printf("\n");

    // DMA allocations
    printf("DMA Allocations:\n");
    printf("  Total allocated:    %s\n", format_bytes(stats.dma_allocated).c_str());
    printf("\n");
}

void MemInfo::print_zones() {
    auto& pmm = PhysicalMemoryManager::instance();
    pmm.dump_zones();
}

void MemInfo::print_free_lists() {
    auto& pmm = PhysicalMemoryManager::instance();
    pmm.dump_free_lists();
}

void MemInfo::print_page_info(uint64_t phys_addr) {
    auto& pmm = PhysicalMemoryManager::instance();
    pmm.dump_page_frame(phys_addr);
}

void MemInfo::print_dma_stats() {
    printf("\n");
    printf("=== DMA Allocator Statistics ===\n");
    printf("\n");
    printf("  Total allocated:   %s\n",
           format_bytes(DMAAllocator::get_total_allocated()).c_str());
    printf("  Available memory:  %s\n",
           format_bytes(DMAAllocator::get_available_memory()).c_str());
    printf("\n");
}

std::string MemInfo::format_bytes(uint64_t bytes) {
    char buffer[64];

    if (bytes >= (1024ULL * 1024 * 1024)) {
        // GB
        double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        snprintf(buffer, sizeof(buffer), "%.2f GB", gb);
    } else if (bytes >= (1024 * 1024)) {
        // MB
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        snprintf(buffer, sizeof(buffer), "%.2f MB", mb);
    } else if (bytes >= 1024) {
        // KB
        double kb = static_cast<double>(bytes) / 1024.0;
        snprintf(buffer, sizeof(buffer), "%.2f KB", kb);
    } else {
        // Bytes
        snprintf(buffer, sizeof(buffer), "%lu bytes", bytes);
    }

    return std::string(buffer);
}

float MemInfo::calculate_fragmentation() {
    auto& pmm = PhysicalMemoryManager::instance();

    uint64_t total_free = pmm.get_free_memory();
    if (total_free == 0) {
        return 0.0f;  // No free memory = no fragmentation
    }

    // Find largest free block across all zones
    uint64_t largest_block = 0;
    for (int zone_idx = 0; zone_idx < 3; zone_idx++) {
        MemoryZone zone = static_cast<MemoryZone>(zone_idx);
        auto* zone_desc = pmm.get_zone(zone);
        if (!zone_desc) continue;

        for (int order = ZoneDescriptor::MAX_ORDER - 1; order >= 0; order--) {
            if (zone_desc->free_area[order].nr_free > 0) {
                uint64_t block_size = (1ULL << order) * 4096;
                if (block_size > largest_block) {
                    largest_block = block_size;
                }
                break;
            }
        }
    }

    // Fragmentation ratio: 1 - (largest_block / total_free)
    // 0.0 = perfect (one contiguous block)
    // 1.0 = highly fragmented (many small blocks)
    if (largest_block == 0) {
        return 1.0f;
    }

    float ratio = 1.0f - (static_cast<float>(largest_block) / static_cast<float>(total_free));
    return (ratio < 0.0f) ? 0.0f : ratio;
}

} // namespace xinim::mm

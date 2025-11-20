/**
 * @file pmm.cpp
 * @brief Physical Memory Manager - Modern Implementation
 * @author XINIM Project
 * @version 2.0
 * @date 2025
 */

#include <xinim/mm/pmm.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <algorithm>

namespace xinim::mm {

// Static instance
PhysicalMemoryManager& PhysicalMemoryManager::instance() {
    static PhysicalMemoryManager instance;
    return instance;
}

PhysicalMemoryManager::PhysicalMemoryManager()
    : total_memory_(0),
      kernel_start_(0),
      kernel_end_(0),
      page_frames_(nullptr),
      total_page_frames_(0),
      total_pages_(0),
      free_pages_(0),
      used_pages_(0),
      reserved_pages_(0) {
}

bool PhysicalMemoryManager::initialize(uint64_t total_memory, uint64_t kernel_start, uint64_t kernel_end) {
    std::lock_guard<std::mutex> lock(global_lock_);

    total_memory_ = total_memory;
    kernel_start_ = kernel_start;
    kernel_end_ = kernel_end;

    total_page_frames_ = total_memory / PAGE_SIZE;
    total_pages_ = total_page_frames_;

    LOG_INFO("PMM: Initializing with %lu MB total memory", total_memory / (1024 * 1024));
    LOG_INFO("PMM: Kernel range: 0x%lx - 0x%lx", kernel_start, kernel_end);

    // Allocate page frame metadata (this comes from early boot allocator)
    size_t metadata_size = total_page_frames_ * sizeof(PageFrame);
    page_frames_ = static_cast<PageFrame*>(::operator new(metadata_size));
    if (!page_frames_) {
        LOG_ERROR("PMM: Failed to allocate page frame metadata");
        return false;
    }

    // Zero out metadata
    memset(page_frames_, 0, metadata_size);

    // Initialize zones
    init_zones();
    init_buddy_allocator();

    LOG_INFO("PMM: Initialization complete");
    LOG_INFO("PMM: DMA zone: %lu pages, NORMAL zone: %lu pages, HIGH zone: %lu pages",
             zones_[0].pages_total, zones_[1].pages_total, zones_[2].pages_total);

    return true;
}

void PhysicalMemoryManager::init_zones() {
    // DMA Zone: 0 - 16MB
    zones_[0].type = MemoryZone::DMA;
    zones_[0].start_pfn = 0;
    zones_[0].end_pfn = DMA_ZONE_END / PAGE_SIZE;
    zones_[0].pages_total = zones_[0].end_pfn - zones_[0].start_pfn;
    zones_[0].pages_free = 0;
    zones_[0].pages_reserved = 0;
    zones_[0].alloc_count = 0;
    zones_[0].free_count = 0;

    // NORMAL Zone: 16MB - 896MB
    zones_[1].type = MemoryZone::NORMAL;
    zones_[1].start_pfn = DMA_ZONE_END / PAGE_SIZE;
    zones_[1].end_pfn = std::min(NORMAL_ZONE_END / PAGE_SIZE, total_page_frames_);
    zones_[1].pages_total = zones_[1].end_pfn - zones_[1].start_pfn;
    zones_[1].pages_free = 0;
    zones_[1].pages_reserved = 0;
    zones_[1].alloc_count = 0;
    zones_[1].free_count = 0;

    // HIGH Zone: >896MB
    zones_[2].type = MemoryZone::HIGH;
    zones_[2].start_pfn = NORMAL_ZONE_END / PAGE_SIZE;
    zones_[2].end_pfn = total_page_frames_;
    zones_[2].pages_total = zones_[2].end_pfn > zones_[2].start_pfn ?
                            zones_[2].end_pfn - zones_[2].start_pfn : 0;
    zones_[2].pages_free = 0;
    zones_[2].pages_reserved = 0;
    zones_[2].alloc_count = 0;
    zones_[2].free_count = 0;

    // Initialize free area structures
    for (auto& zone : zones_) {
        for (size_t order = 0; order < ZoneDescriptor::MAX_ORDER; order++) {
            zone.free_area[order].nr_free = 0;
            zone.free_area[order].free_list.clear();
        }
    }
}

void PhysicalMemoryManager::init_buddy_allocator() {
    // Mark all pages as free initially
    for (uint64_t pfn = 0; pfn < total_page_frames_; pfn++) {
        page_frames_[pfn].flags = PageFrame::FLAG_FREE;
        page_frames_[pfn].ref_count = 0;
        page_frames_[pfn].order = 0;
        page_frames_[pfn].private_data = nullptr;
    }

    // Reserve kernel pages
    uint64_t kernel_start_pfn = phys_to_pfn(kernel_start_);
    uint64_t kernel_end_pfn = phys_to_pfn(kernel_end_);
    for (uint64_t pfn = kernel_start_pfn; pfn < kernel_end_pfn; pfn++) {
        if (pfn < total_page_frames_) {
            page_frames_[pfn].flags = PageFrame::FLAG_RESERVED;
            reserved_pages_++;
        }
    }

    // Reserve first page (null pointer protection)
    page_frames_[0].flags = PageFrame::FLAG_RESERVED;
    reserved_pages_++;

    LOG_INFO("PMM: Reserved %lu pages for kernel and null protection", reserved_pages_);
}

void PhysicalMemoryManager::add_memory_region(uint64_t base, uint64_t size, bool reserved) {
    std::lock_guard<std::mutex> lock(global_lock_);

    uint64_t start_pfn = phys_to_pfn(base);
    uint64_t end_pfn = phys_to_pfn(base + size);

    LOG_INFO("PMM: Adding memory region 0x%lx - 0x%lx (%s)",
             base, base + size, reserved ? "reserved" : "free");

    for (uint64_t pfn = start_pfn; pfn < end_pfn && pfn < total_page_frames_; pfn++) {
        if (page_frames_[pfn].flags & PageFrame::FLAG_RESERVED) {
            continue; // Already reserved (kernel, etc.)
        }

        if (reserved) {
            page_frames_[pfn].flags = PageFrame::FLAG_RESERVED;
            reserved_pages_++;
        } else {
            page_frames_[pfn].flags = PageFrame::FLAG_FREE;
            free_pages_++;

            // Add to appropriate zone's buddy allocator
            uint64_t phys = pfn_to_phys(pfn);
            MemoryZone zone = get_zone_for_address(phys);
            add_to_free_list(phys, 0, zone);
        }
    }
}

uint64_t PhysicalMemoryManager::alloc_page(MemoryZone zone) {
    return buddy_alloc(0, zone);
}

uint64_t PhysicalMemoryManager::alloc_pages(size_t count, MemoryZone zone) {
    if (count == 0) return 0;
    if (count == 1) return alloc_page(zone);

    uint32_t order = get_order(count);
    return buddy_alloc(order, zone);
}

uint64_t PhysicalMemoryManager::alloc_pages_aligned(size_t count, size_t alignment, MemoryZone zone) {
    // For now, use simple allocation and check alignment
    // TODO: Implement proper aligned buddy allocation
    uint64_t addr = alloc_pages(count, zone);
    if (addr && (addr % alignment == 0)) {
        return addr;
    }

    // Not aligned, free and try again (inefficient, but works)
    if (addr) {
        free_pages(addr, count);
    }

    // Try multiple times
    for (int i = 0; i < 10; i++) {
        addr = alloc_pages(count, zone);
        if (addr && (addr % alignment == 0)) {
            return addr;
        }
        if (addr) {
            free_pages(addr, count);
        }
    }

    LOG_ERROR("PMM: Failed to allocate %zu aligned pages", count);
    return 0;
}

uint64_t PhysicalMemoryManager::buddy_alloc(uint32_t order, MemoryZone zone) {
    std::lock_guard<std::mutex> lock(global_lock_);

    ZoneDescriptor* zone_desc = get_zone(zone);
    if (!zone_desc) {
        LOG_ERROR("PMM: Invalid zone");
        return 0;
    }

    std::lock_guard<std::mutex> zone_lock(zone_desc->lock);

    // Try to find a free block of the requested order
    for (uint32_t current_order = order; current_order < ZoneDescriptor::MAX_ORDER; current_order++) {
        if (zone_desc->free_area[current_order].nr_free > 0) {
            // Found a block, remove it from free list
            uint64_t addr = zone_desc->free_area[current_order].free_list.back();
            zone_desc->free_area[current_order].free_list.pop_back();
            zone_desc->free_area[current_order].nr_free--;

            // Split down to requested order
            while (current_order > order) {
                current_order--;
                uint64_t buddy_addr = addr + (get_pages_for_order(current_order) * PAGE_SIZE);
                add_to_free_list(buddy_addr, current_order, zone);
            }

            // Mark pages as allocated
            uint64_t pfn = phys_to_pfn(addr);
            size_t num_pages = get_pages_for_order(order);
            for (size_t i = 0; i < num_pages; i++) {
                if (pfn + i < total_page_frames_) {
                    page_frames_[pfn + i].flags = PageFrame::FLAG_ALLOCATED;
                    page_frames_[pfn + i].ref_count = 1;
                    page_frames_[pfn + i].order = order;
                }
            }

            zone_desc->pages_free -= num_pages;
            zone_desc->alloc_count++;
            free_pages_ -= num_pages;
            used_pages_ += num_pages;

            return addr;
        }
    }

    // Try fallback to other zones if this zone is exhausted
    if (zone == MemoryZone::HIGH) {
        uint64_t addr = buddy_alloc(order, MemoryZone::NORMAL);
        if (addr) return addr;
        return buddy_alloc(order, MemoryZone::DMA);
    } else if (zone == MemoryZone::NORMAL) {
        return buddy_alloc(order, MemoryZone::DMA);
    }

    LOG_ERROR("PMM: Out of memory (zone=%d, order=%u)", static_cast<int>(zone), order);
    return 0;
}

void PhysicalMemoryManager::free_page(uint64_t phys_addr) {
    buddy_free(phys_addr, 0);
}

void PhysicalMemoryManager::free_pages(uint64_t phys_addr, size_t count) {
    if (count == 0) return;
    if (count == 1) {
        free_page(phys_addr);
        return;
    }

    uint32_t order = get_order(count);
    buddy_free(phys_addr, order);
}

void PhysicalMemoryManager::buddy_free(uint64_t phys_addr, uint32_t order) {
    std::lock_guard<std::mutex> lock(global_lock_);

    if (phys_addr == 0) return;

    uint64_t pfn = phys_to_pfn(phys_addr);
    if (pfn >= total_page_frames_) {
        LOG_ERROR("PMM: Invalid physical address 0x%lx", phys_addr);
        return;
    }

    // Mark pages as free
    size_t num_pages = get_pages_for_order(order);
    for (size_t i = 0; i < num_pages; i++) {
        if (pfn + i < total_page_frames_) {
            page_frames_[pfn + i].flags = PageFrame::FLAG_FREE;
            page_frames_[pfn + i].ref_count = 0;
            page_frames_[pfn + i].order = 0;
        }
    }

    MemoryZone zone = get_zone_for_address(phys_addr);
    ZoneDescriptor* zone_desc = get_zone(zone);
    if (!zone_desc) return;

    std::lock_guard<std::mutex> zone_lock(zone_desc->lock);

    // Try to coalesce with buddy
    while (order < ZoneDescriptor::MAX_ORDER - 1) {
        uint64_t buddy_addr = get_buddy_address(phys_addr, order);
        uint64_t buddy_pfn = phys_to_pfn(buddy_addr);

        // Check if buddy is free and at same order
        if (buddy_pfn >= total_page_frames_) break;
        if (!(page_frames_[buddy_pfn].flags & PageFrame::FLAG_FREE)) break;
        if (page_frames_[buddy_pfn].order != order) break;

        // Remove buddy from free list
        remove_from_free_list(buddy_addr, order, zone);

        // Coalesce: keep lower address, increase order
        if (buddy_addr < phys_addr) {
            phys_addr = buddy_addr;
        }
        order++;
    }

    // Add coalesced block to free list
    add_to_free_list(phys_addr, order, zone);

    zone_desc->pages_free += num_pages;
    zone_desc->free_count++;
    free_pages_ += num_pages;
    used_pages_ -= num_pages;
}

PageFrame* PhysicalMemoryManager::get_page_frame(uint64_t phys_addr) {
    uint64_t pfn = phys_to_pfn(phys_addr);
    if (pfn >= total_page_frames_) return nullptr;
    return &page_frames_[pfn];
}

void PhysicalMemoryManager::ref_page(uint64_t phys_addr) {
    PageFrame* frame = get_page_frame(phys_addr);
    if (frame) {
        frame->ref_count++;
    }
}

void PhysicalMemoryManager::unref_page(uint64_t phys_addr) {
    PageFrame* frame = get_page_frame(phys_addr);
    if (frame && frame->ref_count > 0) {
        frame->ref_count--;
        if (frame->ref_count == 0) {
            free_page(phys_addr);
        }
    }
}

ZoneDescriptor* PhysicalMemoryManager::get_zone(MemoryZone zone) {
    int idx = static_cast<int>(zone);
    if (idx >= 0 && idx < NUM_ZONES) {
        return &zones_[idx];
    }
    return nullptr;
}

MemoryZone PhysicalMemoryManager::get_zone_for_address(uint64_t phys_addr) {
    if (phys_addr < DMA_ZONE_END) {
        return MemoryZone::DMA;
    } else if (phys_addr < NORMAL_ZONE_END) {
        return MemoryZone::NORMAL;
    } else {
        return MemoryZone::HIGH;
    }
}

uint64_t PhysicalMemoryManager::get_free_memory() const {
    return free_pages_ * PAGE_SIZE;
}

uint64_t PhysicalMemoryManager::get_used_memory() const {
    return used_pages_ * PAGE_SIZE;
}

uint64_t PhysicalMemoryManager::get_reserved_memory() const {
    return reserved_pages_ * PAGE_SIZE;
}

uint32_t PhysicalMemoryManager::get_order(size_t pages) {
    if (pages == 0) return 0;

    uint32_t order = 0;
    size_t size = 1;
    while (size < pages && order < ZoneDescriptor::MAX_ORDER - 1) {
        size <<= 1;
        order++;
    }
    return order;
}

void PhysicalMemoryManager::add_to_free_list(uint64_t phys_addr, uint32_t order, MemoryZone zone) {
    ZoneDescriptor* zone_desc = get_zone(zone);
    if (!zone_desc || order >= ZoneDescriptor::MAX_ORDER) return;

    zone_desc->free_area[order].free_list.push_back(phys_addr);
    zone_desc->free_area[order].nr_free++;

    // Mark page frame order
    uint64_t pfn = phys_to_pfn(phys_addr);
    if (pfn < total_page_frames_) {
        page_frames_[pfn].order = order;
    }
}

void PhysicalMemoryManager::remove_from_free_list(uint64_t phys_addr, uint32_t order, MemoryZone zone) {
    ZoneDescriptor* zone_desc = get_zone(zone);
    if (!zone_desc || order >= ZoneDescriptor::MAX_ORDER) return;

    auto& free_list = zone_desc->free_area[order].free_list;
    auto it = std::find(free_list.begin(), free_list.end(), phys_addr);
    if (it != free_list.end()) {
        free_list.erase(it);
        zone_desc->free_area[order].nr_free--;
    }
}

uint64_t PhysicalMemoryManager::get_buddy_address(uint64_t phys_addr, uint32_t order) {
    uint64_t block_size = get_pages_for_order(order) * PAGE_SIZE;
    return phys_addr ^ block_size;
}

void PhysicalMemoryManager::dump_zones() {
    LOG_INFO("=== PMM Zone Information ===");
    for (const auto& zone : zones_) {
        const char* zone_name = "UNKNOWN";
        if (zone.type == MemoryZone::DMA) zone_name = "DMA";
        else if (zone.type == MemoryZone::NORMAL) zone_name = "NORMAL";
        else if (zone.type == MemoryZone::HIGH) zone_name = "HIGH";

        LOG_INFO("Zone %s:", zone_name);
        LOG_INFO("  Range: PFN %lu - %lu", zone.start_pfn, zone.end_pfn);
        LOG_INFO("  Total: %lu pages (%lu MB)", zone.pages_total,
                 pages_to_bytes(zone.pages_total) / (1024 * 1024));
        LOG_INFO("  Free: %lu pages (%lu MB)", zone.pages_free,
                 pages_to_bytes(zone.pages_free) / (1024 * 1024));
        LOG_INFO("  Reserved: %lu pages", zone.pages_reserved);
        LOG_INFO("  Alloc count: %lu, Free count: %lu", zone.alloc_count, zone.free_count);
    }
}

void PhysicalMemoryManager::dump_page_frame(uint64_t phys_addr) {
    PageFrame* frame = get_page_frame(phys_addr);
    if (!frame) {
        LOG_ERROR("PMM: Invalid page frame address 0x%lx", phys_addr);
        return;
    }

    LOG_INFO("Page Frame at 0x%lx:", phys_addr);
    LOG_INFO("  Flags: 0x%x", frame->flags);
    LOG_INFO("  Ref count: %u", frame->ref_count);
    LOG_INFO("  Order: %u", frame->order);
    LOG_INFO("  Private data: %p", frame->private_data);
}

void PhysicalMemoryManager::dump_free_lists() {
    LOG_INFO("=== PMM Free Lists ===");
    for (size_t zone_idx = 0; zone_idx < NUM_ZONES; zone_idx++) {
        const auto& zone = zones_[zone_idx];
        const char* zone_name = "UNKNOWN";
        if (zone.type == MemoryZone::DMA) zone_name = "DMA";
        else if (zone.type == MemoryZone::NORMAL) zone_name = "NORMAL";
        else if (zone.type == MemoryZone::HIGH) zone_name = "HIGH";

        LOG_INFO("Zone %s:", zone_name);
        for (uint32_t order = 0; order < ZoneDescriptor::MAX_ORDER; order++) {
            if (zone.free_area[order].nr_free > 0) {
                LOG_INFO("  Order %u: %u blocks (%zu pages each)",
                         order, zone.free_area[order].nr_free,
                         get_pages_for_order(order));
            }
        }
    }
}

} // namespace xinim::mm

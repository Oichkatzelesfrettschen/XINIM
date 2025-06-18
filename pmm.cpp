#include "pmm.h"
#include "console.h" // For debug messages (optional, consider a dedicated kprint)
#include <stdint.h>
#include <stddef.h> // For NULL, size_t

// Bitmap for page frames
static uint32_t* page_bitmap = NULL;
static size_t total_pages = 0;
static size_t used_pages = 0;
static uintptr_t highest_address = 0;

// Helper to set a bit in the bitmap
static inline void bitmap_set(size_t bit) {
    page_bitmap[bit / 32] |= (1 << (bit % 32));
}

// Helper to clear a bit in the bitmap
static inline void bitmap_clear(size_t bit) {
    page_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

// Helper to test if a bit is set
static inline bool bitmap_test(size_t bit) {
    return (page_bitmap[bit / 32] & (1 << (bit % 32))) != 0;
}

// Marks a range of pages as used (allocates them)
static void pmm_mark_region_used(uintptr_t base_addr, size_t size_in_bytes) {
    uintptr_t aligned_base = base_addr / PAGE_SIZE;
    uintptr_t aligned_end = (base_addr + size_in_bytes -1) / PAGE_SIZE;

    for (uintptr_t page = aligned_base; page <= aligned_end && page < total_pages; ++page) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
    }
}

// Marks a range of pages as free
static void pmm_mark_region_free(uintptr_t base_addr, size_t size_in_bytes) {
    uintptr_t aligned_base = base_addr / PAGE_SIZE;
    uintptr_t aligned_end = (base_addr + size_in_bytes - 1) / PAGE_SIZE;

    for (uintptr_t page = aligned_base; page <= aligned_end && page < total_pages; ++page) {
        if (bitmap_test(page)) { // Only free if it was used
            bitmap_clear(page);
            used_pages--; // This should be an assertion, but for init it's ok
        }
    }
}


void pmm_init(struct multiboot_tag_mmap* mmap_tag, uintptr_t kernel_start_phys, uintptr_t kernel_end_phys, uintptr_t multiboot_info_addr_phys) {
    if (!mmap_tag) {
        console_write_string("PMM Error: No memory map provided!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        return;
    }

    // 1. First pass: Calculate total memory and find the highest address to determine bitmap size
    highest_address = 0;
    uintptr_t total_ram_bytes = 0;
    struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)((uint8_t*)mmap_tag + sizeof(struct multiboot_tag_mmap) - sizeof(struct multiboot_mmap_entry) + mmap_tag->entry_size); // Point to first entry
    // The above line is a bit complex due to flexible mmap_tag structure. A simpler way:
    entry = (struct multiboot_mmap_entry*)( (uintptr_t)mmap_tag + sizeof(uint32_t) * 4); // type, size, entry_size, entry_version

    for (unsigned i = 0; i < (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size; ++i) {
        if (entry[i].addr + entry[i].len > highest_address) {
            highest_address = entry[i].addr + entry[i].len;
        }
        if (entry[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
             total_ram_bytes += entry[i].len;
        }
    }

    total_pages = highest_address / PAGE_SIZE;
    if (highest_address % PAGE_SIZE != 0) total_pages++; // Account for partial last page

    size_t bitmap_size_bytes = (total_pages + 7) / 8; // +7 for ceiling division
    // size_t bitmap_size_pages = (bitmap_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE; // Unused


    // 2. Second pass: Find a suitable region for the bitmap itself.
    // We need `bitmap_size_bytes` of *contiguous* available physical memory.
    // The bitmap should preferably be placed after the kernel.
    uintptr_t bitmap_phys_addr = 0;
    entry = (struct multiboot_mmap_entry*)( (uintptr_t)mmap_tag + sizeof(uint32_t) * 4);
    for (unsigned i = 0; i < (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size; ++i) {
        if (entry[i].type == MULTIBOOT_MEMORY_AVAILABLE && entry[i].len >= bitmap_size_bytes) {
            // Check if this region is suitable (e.g., above 1MB and not overlapping kernel too much)
            uintptr_t potential_addr = entry[i].addr;
            if (potential_addr < 0x100000) { // Avoid areas below 1MB if possible, or areas too small
                 if (entry[i].addr + entry[i].len > 0x100000) { // If it spans across 1MB
                    potential_addr = 0x100000;
                 } else {
                    continue; // Too low and too small
                 }
            }

            // A simple strategy: take the first available spot that's large enough and ideally after kernel_end_phys
            // A more robust strategy would check for overlap with kernel explicitly or prefer highest possible address.
            if (potential_addr + bitmap_size_bytes <= entry[i].addr + entry[i].len) {
                 // Ensure it doesn't overlap kernel OR is placed right after the kernel
                bool overlaps_kernel = (potential_addr < kernel_end_phys && potential_addr + bitmap_size_bytes > kernel_start_phys);
                if (!overlaps_kernel || potential_addr >= kernel_end_phys) {
                     bitmap_phys_addr = potential_addr;
                     // If we found a spot after the kernel, try to use it.
                     // Otherwise, we might need to search for any spot.
                     // For now, take the first good one.
                     if (potential_addr >= kernel_end_phys) break;
                }
            }
        }
    }

    if (bitmap_phys_addr == 0) {
        // Fallback: Try to find space for bitmap *anywhere* available if not found after kernel
        entry = (struct multiboot_mmap_entry*)( (uintptr_t)mmap_tag + sizeof(uint32_t) * 4);
        for (unsigned i = 0; i < (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size; ++i) {
            if (entry[i].type == MULTIBOOT_MEMORY_AVAILABLE && entry[i].len >= bitmap_size_bytes) {
                uintptr_t potential_addr = entry[i].addr;
                 // Check if this region is suitable (e.g., above 1MB or known safe low areas if desperate)
                if (potential_addr < 0x1000 && entry[i].len < bitmap_size_bytes) continue; // Skip very low memory unless large enough
                if (potential_addr + bitmap_size_bytes <= entry[i].addr + entry[i].len) {
                    bool overlaps_kernel = (potential_addr < kernel_end_phys && potential_addr + bitmap_size_bytes > kernel_start_phys);
                    if (!overlaps_kernel) { // Must not overlap kernel if not placed after
                        bitmap_phys_addr = potential_addr;
                        break;
                    }
                }
            }
        }
    }


    if (bitmap_phys_addr == 0) {
        console_write_string("PMM Error: Not enough continuous memory for bitmap!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        // This is a fatal error. Potentially halt or panic.
        return;
    }
    page_bitmap = (uint32_t*)bitmap_phys_addr; // For now, direct map. VMM will need to map this.

    // 3. Initialize bitmap: mark all pages as used initially.
    used_pages = total_pages;
    for (size_t i = 0; i < (bitmap_size_bytes / sizeof(uint32_t)); ++i) {
        page_bitmap[i] = 0xFFFFFFFF; // All bits set (all pages used)
    }
    // Clear any remaining bits in the last dword if bitmap_size_bytes is not a multiple of 4
    if (bitmap_size_bytes % sizeof(uint32_t) != 0) {
        size_t remainder_bits = total_pages % 32;
        if(remainder_bits > 0) {
             page_bitmap[bitmap_size_bytes / sizeof(uint32_t)] = (1 << remainder_bits) -1 ; // Set only relevant bits
        } else {
             page_bitmap[bitmap_size_bytes / sizeof(uint32_t)] = 0xFFFFFFFF;
        }

    }


    // 4. Mark available regions as free based on memory map
    entry = (struct multiboot_mmap_entry*)( (uintptr_t)mmap_tag + sizeof(uint32_t) * 4);
    for (unsigned i = 0; i < (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size; ++i) {
        if (entry[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            // console_write_string("  Mem Avail: 0x", DEFAULT_COLOR); console_write_hex(entry[i].addr, DEFAULT_COLOR);
            // console_write_string(" Len: 0x", DEFAULT_COLOR); console_write_hex(entry[i].len, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
            pmm_mark_region_free(entry[i].addr, entry[i].len);
        }
    }

    // 5. Mark reserved regions as used:
    // - Region below 1MB (0 to 0xFFFFF) often has BIOS, VGA buffer etc. Some parts are available.
    //   For simplicity, mark it all used first, then specific available parts from mmap are freed above.
    //   A more precise approach is to only mark reserved sections from mmap as used if not already.
    //   We've already marked everything used, and then freed available sections. So reserved sections remain marked used.

    // - Kernel code and data
    pmm_mark_region_used(kernel_start_phys, kernel_end_phys - kernel_start_phys);

    // - The bitmap itself
    pmm_mark_region_used(bitmap_phys_addr, bitmap_size_bytes);

    // - Multiboot info structure (assuming its size is roughly known or small, e.g., 1 page)
    //   The exact size of the multiboot structure can be found by iterating all tags.
    //   For now, reserve a page for simplicity if it's not already covered.
    //   A better way: get the full size of the multiboot info. For now, assume it's small.
    uintptr_t mb_info_end = multiboot_info_addr_phys + PAGE_SIZE; // Guessing one page for MB info
    struct multiboot_tag* tag = (struct multiboot_tag*)(multiboot_info_addr_phys + 8); // First tag after fixed header
    for (; tag->type != MULTIBOOT_TAG_TYPE_END; tag = (struct multiboot_tag*)((uint8_t*)tag + multiboot_tag_align(tag->size))) {
         // Loop to find the end
    }
    mb_info_end = (uintptr_t)tag + sizeof(struct multiboot_tag) ; // End of last tag
    pmm_mark_region_used(multiboot_info_addr_phys, mb_info_end - multiboot_info_addr_phys);


    console_write_string("PMM Initialized. Total Pages: ", DEFAULT_COLOR); console_write_dec(total_pages, DEFAULT_COLOR);
    console_write_string(" (", DEFAULT_COLOR); console_write_dec(total_pages * PAGE_SIZE / (1024*1024), DEFAULT_COLOR); console_write_string("MB)\n", DEFAULT_COLOR);
    console_write_string("Used Pages: ", DEFAULT_COLOR); console_write_dec(used_pages, DEFAULT_COLOR);
    console_write_string(" (Bitmap @ 0x", DEFAULT_COLOR); console_write_hex(bitmap_phys_addr, DEFAULT_COLOR);
    console_write_string(", Size: ", DEFAULT_COLOR); console_write_dec(bitmap_size_bytes / 1024, DEFAULT_COLOR); console_write_string(" KB)\n", DEFAULT_COLOR);
    console_write_string("Free Pages: ", DEFAULT_COLOR); console_write_dec(total_pages - used_pages, DEFAULT_COLOR);
    console_write_char('\n', DEFAULT_COLOR);
}


uintptr_t pmm_alloc_page() {
    if (!page_bitmap) return 0; // PMM not initialized, return 0 as invalid address

    // Simple first-fit search
    for (size_t i = 0; i < total_pages; ++i) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            return (uintptr_t)(i * PAGE_SIZE);
        }
    }
    console_write_string("PMM: Out of memory!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
    return 0; // No free page found, return 0 as invalid address
}

void pmm_free_page(uintptr_t page_addr) {
    if (!page_bitmap || page_addr % PAGE_SIZE != 0) {
        // Optionally, add error message for unaligned address or PMM not init
        return;
    }

    size_t page_index = page_addr / PAGE_SIZE;
    if (page_index >= total_pages) {
        // Optionally, add error message for out-of-bounds page
        return;
    }

    if (bitmap_test(page_index)) { // Only free if it was allocated
        bitmap_clear(page_index);
        used_pages--;
    } else {
        // Optionally, add warning for trying to free an already free page
    }
}

size_t pmm_get_total_pages() {
    return total_pages;
}

size_t pmm_get_used_pages() {
    return used_pages;
}

size_t pmm_get_free_pages() {
    return total_pages - used_pages;
}

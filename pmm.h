#ifndef PMM_H
#define PMM_H

#include "multiboot.h" // For multiboot_tag_mmap
#include <stddef.h>    // For size_t
#include <stdint.h>    // For uint32_t, etc.

#define PAGE_SIZE 4096 // 4KB pages
#define PMM_INVALID_PAGE ( (uint32_t)-1 ) // Represents an invalid page or allocation failure

// Initializes the Physical Memory Manager with the memory map from Multiboot2.
// kernel_start and kernel_end are physical addresses of the loaded kernel.
// multiboot_info_addr is the physical address of the multiboot info structure.
void pmm_init(struct multiboot_tag_mmap* mmap_tag, uintptr_t kernel_start_phys, uintptr_t kernel_end_phys, uintptr_t multiboot_info_addr_phys);

// Allocates a single physical page frame.
// Returns the physical address of the allocated page, or NULL if no free page is available.
uintptr_t pmm_alloc_page();

// Frees a single physical page frame.
// page_addr must be a 4KB aligned physical address previously allocated by pmm_alloc_page.
void pmm_free_page(uintptr_t page_addr);

// Returns the total number of physical pages.
size_t pmm_get_total_pages();

// Returns the number of used physical pages.
size_t pmm_get_used_pages();

// Returns the number of free physical pages.
size_t pmm_get_free_pages();

#endif // PMM_H

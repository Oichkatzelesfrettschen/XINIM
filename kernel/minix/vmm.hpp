#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h> // For size_t

// Page Directory Entry (PDE) and Page Table Entry (PTE) Flags
#define PTE_PRESENT       0x01   // Present
#define PTE_READ_WRITE    0x02   // Read/Write
#define PTE_USER          0x04   // User/Supervisor
#define PTE_WRITE_THROUGH 0x08   // Write-Through caching
#define PTE_CACHE_DISABLE 0x10   // Cache Disabled
#define PTE_ACCESSED      0x20   // Accessed
#define PTE_DIRTY         0x40   // Dirty (PTE only)
#define PTE_PAT           0x80   // Page Attribute Table (PTE only, or PDE PS=1)
#define PTE_GLOBAL        0x100  // Global page (PTE only, ignored if CR4.PGE=0)

#define PDE_PAGE_SIZE     0x80   // Page Size (1 = 4MB page)


// A Page Table Entry (PTE) points to a 4KB physical page.
typedef uint32_t pte_t;

// A Page Directory Entry (PDE) points to a page table.
typedef uint32_t pde_t;

// A Page Directory is an array of 1024 PDEs. (4KB)
// A Page Table is an array of 1024 PTEs. (4KB)
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_DIR_ENTRIES   1024

#define PAGE_SIZE_4KB 0x1000
#define PAGE_SIZE_4MB 0x400000

// Kernel virtual base address (if using higher half kernel)
// For now, assume kernel is loaded at 1MB and identity mapped.
// If kernel is at 0xC0100000, this would be 0xC0000000.
// For identity mapping at 1MB, this is effectively 0.
#define KERNEL_VIRTUAL_BASE 0x00000000

// Function to get physical address from a PDE or PTE
static inline uintptr_t pte_get_addr(pte_t entry) {
    return entry & ~0xFFF; // Mask out flags (lower 12 bits)
}

// Function to set physical address in a PDE or PTE
static inline void pte_set_addr(pte_t* entry, uintptr_t addr) {
    *entry = (*entry & 0xFFF) | (addr & ~0xFFF);
}

// Function to set flags in a PDE or PTE
static inline void pte_set_flags(pte_t* entry, uint32_t flags) {
    *entry |= flags;
}

// Initializes the Virtual Memory Manager.
// kernel_phys_start, kernel_phys_end: physical memory bounds of the kernel.
// kernel_virt_start, kernel_virt_end: virtual memory bounds of the kernel.
// For identity mapping, phys_start == virt_start and phys_end == virt_end.
void vmm_init(uintptr_t kernel_phys_start, uintptr_t kernel_phys_end,
              uintptr_t kernel_virt_start, uintptr_t kernel_virt_end);

// Maps a virtual page to a physical page.
bool vmm_map_page(uintptr_t virtual_addr, uintptr_t physical_addr, uint32_t flags);

// Unmaps a virtual page.
void vmm_unmap_page(uintptr_t virtual_addr);

// Gets the PTE for a virtual address (for modification or inspection).
// If create is true, page tables will be allocated if they don't exist.
pte_t* vmm_get_pte(uintptr_t virtual_addr, bool create);

// Loads the given page directory into CR3.
void vmm_load_page_directory(uintptr_t page_dir_phys_addr);

// Enables paging by setting CR0.PG.
void vmm_enable_paging();

// Returns the physical address of the current (initial) page directory.
uintptr_t vmm_get_page_directory_physical_addr();

// Helper to align down to page boundary
static inline uintptr_t align_down(uintptr_t addr, size_t alignment) {
    return addr & ~(alignment - 1);
}

// Helper to align up to page boundary
static inline uintptr_t align_up(uintptr_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}


#endif // VMM_H

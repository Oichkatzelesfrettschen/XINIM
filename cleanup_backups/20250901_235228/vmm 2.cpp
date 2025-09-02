#include "vmm.h"
#include "pmm.h"
#include "console.hpp" // For debug messages
#include <stddef.h> // For NULL
#include <stdint.h> // For uintptr_t

// The kernel's page directory
static pde_t* kernel_page_directory = NULL;
static uintptr_t kernel_page_directory_phys = 0;

// External symbols from linker script, defining kernel's physical and virtual layout
// These need to be defined in your linker script (e.g., linker.ld)
// extern "C" uint32_t kernel_physical_start; // Filled by linker
// extern "C" uint32_t kernel_physical_end;   // Filled by linker
// extern "C" uint32_t kernel_virtual_start;  // Filled by linker
// extern "C" uint32_t kernel_virtual_end;    // Filled by linker
// For now, we pass these as parameters to vmm_init.

void vmm_init(uintptr_t kernel_phys_start, uintptr_t kernel_phys_end,
              uintptr_t kernel_virt_start, uintptr_t /*kernel_virt_end*/) { // kernel_virt_end removed as unused
    // 1. Allocate a page frame for the kernel page directory from PMM
    kernel_page_directory_phys = pmm_alloc_page();
    if (kernel_page_directory_phys == 0) { // Compare with 0 for uintptr_t
        console_write_string("VMM Error: Failed to allocate page for page directory!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        // PANIC or handle error
        return;
    }
    // For now, the PMM returns physical addresses, and we need a virtual address to work with it.
    // Before paging is enabled, physical == virtual for this newly allocated page (if < 4GB and not remapped by bootloader).
    // Once paging is enabled, this direct mapping might not be valid unless we explicitly map it.
    // A common approach is to temporarily map the page directory to modify it or have a region of memory (e.g. first 4MB) identity mapped.
    // For simplicity during init, we assume we can directly access kernel_page_directory_phys.
    // This will need to be mapped if PMM gives pages outside current identity map.
    // Let's assume PMM gives an address we can write to (e.g. it's within the initial identity map or kernel space).
    // A robust solution would map this page directory to a known virtual address.
    // For now, if kernel is at 1MB, and PMM allocates from there, it's fine initially.
    kernel_page_directory = (pde_t*)kernel_page_directory_phys;


    // 2. Initialize the page directory: clear all entries (mark not present)
    for (int i = 0; i < PAGE_DIR_ENTRIES; ++i) {
        kernel_page_directory[i] = 0; // Not present, R/W, Supervisor
    }

    // 3. Identity map the kernel's code and data sections
    // Align addresses to page boundaries
    uintptr_t phys_start_aligned = align_down(kernel_phys_start, PAGE_SIZE_4KB);
    uintptr_t phys_end_aligned = align_up(kernel_phys_end, PAGE_SIZE_4KB);
    uintptr_t virt_start_aligned = align_down(kernel_virt_start, PAGE_SIZE_4KB);
    // uintptr_t virt_end_aligned = align_up(kernel_virt_end, PAGE_SIZE_4KB); // Not directly used in loop below

    console_write_string("VMM: Mapping Kernel from Phys 0x", DEFAULT_COLOR); console_write_hex(phys_start_aligned, DEFAULT_COLOR);
    console_write_string(" to Virt 0x", DEFAULT_COLOR); console_write_hex(virt_start_aligned, DEFAULT_COLOR);
    console_write_string(" (Size: ", DEFAULT_COLOR); console_write_dec((phys_end_aligned - phys_start_aligned)/(1024), DEFAULT_COLOR); console_write_string(" KB)\n", DEFAULT_COLOR);

    for (uintptr_t paddr = phys_start_aligned, vaddr = virt_start_aligned;
         paddr < phys_end_aligned;
         paddr += PAGE_SIZE_4KB, vaddr += PAGE_SIZE_4KB) {
        if (!vmm_map_page(vaddr, paddr, PTE_PRESENT | PTE_READ_WRITE)) { // Kernel is R/W for now
            console_write_string("VMM Error: Failed to map kernel page VA:0x", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            console_write_hex(vaddr, vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            console_write_string(" PA:0x", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            console_write_hex(paddr, vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            console_write_char('\n', vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            // PANIC or handle
            return;
        }
    }

    // 4. Map the VGA buffer (physical 0xB8000)
    // Assuming VGA buffer is 80x25 characters, 2 bytes per char = 4000 bytes, fits in one 4KB page.
    // We need to decide its virtual address.
    // If kernel is at 1MB (0x100000), 0xB8000 is below it and can be identity mapped.
    // If kernel is higher-half (e.g. 0xC0000000), VGA is often mapped to KERNEL_VIRTUAL_BASE + 0xB8000 or similar.
    // For now, identity map it.
    uintptr_t vga_phys_addr = 0xB8000;
    uintptr_t vga_virt_addr = 0xB8000; // Identity mapping for now

    // Also map the page containing the page directory itself, so we can modify it after paging is enabled
    // This is crucial if kernel_page_directory was a physical address that might not be covered by kernel mapping.
    // However, if PMM allocated it from within the kernel's future mapped region, this might be redundant but safe.
    if (!vmm_map_page(kernel_page_directory_phys, kernel_page_directory_phys, PTE_PRESENT | PTE_READ_WRITE)) {
         console_write_string("VMM Error: Failed to map page directory itself!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
         return;
    }


    console_write_string("VMM: Mapping VGA buffer Phys 0xB8000 to Virt 0x", DEFAULT_COLOR); console_write_hex(vga_virt_addr, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
    if (!vmm_map_page(vga_virt_addr, vga_phys_addr, PTE_PRESENT | PTE_READ_WRITE)) {
        console_write_string("VMM Error: Failed to map VGA buffer!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        // PANIC or handle
        return;
    }

    // Update console's VGA buffer pointer if it's going to change
    // For identity map, it doesn't change from 0xB8000.
    // If it were, e.g., 0xC00B8000: console_set_vga_buffer_address((unsigned short*)0xC00B8000);
    // This needs to be done *after* paging is enabled if the new address isn't accessible yet.
    // For now, we assume console will use 0xB8000 which we just mapped.

    console_write_string("VMM Initialized. Page Directory @ Phys 0x", DEFAULT_COLOR); console_write_hex(kernel_page_directory_phys, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
}

pte_t* vmm_get_pte(uintptr_t virtual_addr, bool create_if_missing) {
    if (!kernel_page_directory) return NULL;

    uintptr_t page_dir_index = (virtual_addr >> 22);          // Top 10 bits for PD index
    uintptr_t page_table_index = (virtual_addr >> 12) & 0x3FF; // Middle 10 bits for PT index

    pde_t pde = kernel_page_directory[page_dir_index];

    if (!(pde & PTE_PRESENT)) {
        if (create_if_missing) {
            uintptr_t new_page_table_phys = pmm_alloc_page();
            if (new_page_table_phys == 0) { // Compare with 0 for uintptr_t
                console_write_string("VMM: Failed to allocate page for page table!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
                return NULL; // Out of memory
            }
            // Initialize the new page table (all entries not present)
            pte_t* new_page_table_virt = (pte_t*)new_page_table_phys; // Assuming temp identity map for new PT
                                                                      // This is a critical point: new_page_table_phys needs to be writable.
                                                                      // If PMM returns pages outside kernel's initial map, this needs careful handling.
                                                                      // For now, assume PMM gives usable physical addresses that can be accessed.
                                                                      // This new page table itself needs to be mapped if we want to write to it *after* paging is enabled
                                                                      // and it's outside the initial identity map.
                                                                      // One solution: map it temporarily, or ensure PMM allocates from a mapped region.

            for (int i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
                new_page_table_virt[i] = 0; // Not present, R/W, Supervisor
            }

            kernel_page_directory[page_dir_index] = new_page_table_phys | PTE_PRESENT | PTE_READ_WRITE | PTE_USER; // Mark PD entry: present, R/W, User (can be overridden by PTE)
            pde = kernel_page_directory[page_dir_index];
        } else {
            return NULL; // Page table does not exist, and not creating
        }
    }

    // Page table exists, get its virtual address to access its entries
    // The physical address is stored in PDE. We need a virtual address.
    // If page tables are allocated within the kernel's mapped region, this is simpler.
    // If they are elsewhere, they too must be mapped.
    // For now, assume pte_get_addr(pde) gives a physical address that is also accessible directly.
    pte_t* page_table_virt = (pte_t*)pte_get_addr(pde);
    return &page_table_virt[page_table_index];
}

bool vmm_map_page(uintptr_t virtual_addr, uintptr_t physical_addr, uint32_t flags) {
    // Align addresses to page boundaries
    virtual_addr = align_down(virtual_addr, PAGE_SIZE_4KB);
    physical_addr = align_down(physical_addr, PAGE_SIZE_4KB);

    pte_t* pte = vmm_get_pte(virtual_addr, true /* create page table if missing */);
    if (!pte) {
        return false; // Failed to get/create PTE (likely OOM for page table)
    }

    if (*pte & PTE_PRESENT) {
        // Page already mapped, could be an error or re-mapping.
        // For simplicity, we'll overwrite. A real VMM might free the old frame if physical_addr changes.
        // console_write_string("VMM Warning: Remapping page VA:0x", DEFAULT_COLOR); console_write_hex(virtual_addr, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
    }

    *pte = physical_addr | flags;
    return true;
}

void vmm_unmap_page(uintptr_t virtual_addr) {
    pte_t* pte = vmm_get_pte(virtual_addr, false /* don't create */);
    if (pte && (*pte & PTE_PRESENT)) {
        // Mark page as not present.
        // Note: This doesn't free the physical frame via PMM. That's a higher-level decision.
        *pte = 0;

        // Invalidate TLB entry for this virtual address (optional, but good practice)
        // asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
        // For now, rely on CR3 reload to flush TLB.
    }
}

void vmm_load_page_directory(uintptr_t page_dir_phys_addr) {
    kernel_page_directory_phys = page_dir_phys_addr; // Store it if changed by someone else
    asm volatile("mov %0, %%cr3" :: "r"(page_dir_phys_addr) : "memory");
}

void vmm_enable_paging() {
    asm volatile(
        "mov %%cr0, %%eax;"
        "or $0x80000000, %%eax;" // Set PG bit (bit 31)
        "mov %%eax, %%cr0;"
        ::: "eax", "memory"
    );
}

uintptr_t vmm_get_page_directory_physical_addr() {
    return kernel_page_directory_phys;
}

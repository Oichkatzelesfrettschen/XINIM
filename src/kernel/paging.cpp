#include "../../include/paging.hpp"
#include "const.hpp"               
#include "../mm/alloc.hpp"
#include "sys/type.hpp"           
#include "xinim/boot/bootinfo.hpp"
#include "proc.hpp"
#include <cstdint>                  
#include <cstddef>                  
#include <cstring>                  

namespace xinim::boot {
    extern const BootInfo& get_info();
}

static struct pml4 kernel_pml4; 
static uint64_t next_kernel_va;   

/**
 * @brief Helper to convert physical address to virtual via HHDM.
 */
static inline void* phys_to_virt(uint64_t pa) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(pa + xinim::boot::get_info().hhdm_offset));
}

static int map_page_generic(struct pml4* pml4_v, uint64_t va, uint64_t pa, int flags) noexcept;

PUBLIC void paging_init() {
    size_t i;
    for (i = 0; i < PT_ENTRIES; i++)
        kernel_pml4.ptrs[i] = nullptr; 
    next_kernel_va = UINT64_C(0xffff800000000000); 
}

PUBLIC void *alloc_virtual(uint64_t bytes, int flags) {
    uint64_t va = next_kernel_va;
    uint64_t pages = (bytes + static_cast<uint64_t>(PAGE_SIZE_4K) - 1) / static_cast<uint64_t>(PAGE_SIZE_4K);

    // Allocate physical frames and create page table entries for each page
    for (uint64_t i = 0; i < pages; ++i) {
        uint64_t page_va = va + i * static_cast<uint64_t>(PAGE_SIZE_4K);
        // Allocate one physical page (1 click = PAGE_SIZE_4K when CLICK_SHIFT matches)
        uint64_t phys_clicks = alloc_mem(1);
        if (phys_clicks == 0) {
            return nullptr; // out of physical memory
        }
        uint64_t pa = phys_clicks << CLICK_SHIFT;
        int map_flags = static_cast<int>(static_cast<unsigned int>(flags) | PT_PRESENT | PT_WRITABLE);
        if (map_page_generic(&kernel_pml4, page_va, pa, map_flags) != xinim::OK) {
            return nullptr; // page table allocation failed
        }
    }

    next_kernel_va += pages * static_cast<uint64_t>(PAGE_SIZE_4K);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(va));
}

static int map_page_generic(struct pml4* pml4_v, uint64_t va, uint64_t pa, int flags) noexcept {
    auto idx4 = static_cast<unsigned int>((va >> 39) & 0x1FF);
    auto idx3 = static_cast<unsigned int>((va >> 30) & 0x1FF);
    auto idx2 = static_cast<unsigned int>((va >> 21) & 0x1FF);
    auto idx1 = static_cast<unsigned int>((va >> 12) & 0x1FF);

    if (!pml4_v->ptrs[idx4]) {
        uint64_t p_addr_clicks = alloc_mem((sizeof(struct page_dir_ptr) + CLICK_SIZE - 1) >> CLICK_SHIFT);
        if (p_addr_clicks == 0) return -1;
        pml4_v->ptrs[idx4] = static_cast<struct page_dir_ptr*>(phys_to_virt(p_addr_clicks << CLICK_SHIFT));
        memset(pml4_v->ptrs[idx4], 0, sizeof(struct page_dir_ptr));
    }

    struct page_dir_ptr* pdpt = pml4_v->ptrs[idx4];
    if (!pdpt->dirs[idx3]) {
        uint64_t p_addr_clicks = alloc_mem((sizeof(struct page_directory) + CLICK_SIZE - 1) >> CLICK_SHIFT);
        if (p_addr_clicks == 0) return -1;
        pdpt->dirs[idx3] = static_cast<struct page_directory*>(phys_to_virt(p_addr_clicks << CLICK_SHIFT));
        memset(pdpt->dirs[idx3], 0, sizeof(struct page_directory));
    }

    struct page_directory* pd = pdpt->dirs[idx3];
    if (!pd->tables[idx2]) {
        uint64_t p_addr_clicks = alloc_mem((sizeof(struct page_table) + CLICK_SIZE - 1) >> CLICK_SHIFT);
        if (p_addr_clicks == 0) return -1;
        pd->tables[idx2] = static_cast<struct page_table*>(phys_to_virt(p_addr_clicks << CLICK_SHIFT));
        memset(pd->tables[idx2], 0, sizeof(struct page_table));
    }

    struct page_table* pt = pd->tables[idx2];
    pt->entries[idx1].addr = pa;
    pt->entries[idx1].flags = static_cast<unsigned long>(flags) | PT_PRESENT;

    return xinim::OK;
}

PUBLIC int map_page(uint64_t va, uint64_t pa, int flags) noexcept {
    return map_page_generic(&kernel_pml4, va, pa, flags);
}

PUBLIC int map_page_to_process(xinim::pid_t pid, uint64_t va, uint64_t pa, int flags) noexcept {
    struct proc* rp = proc_addr(pid);
    if (!rp || rp->cr3 == 0) return -1;
    return map_page_generic(static_cast<struct pml4*>(phys_to_virt(rp->cr3)), va, pa, flags);
}

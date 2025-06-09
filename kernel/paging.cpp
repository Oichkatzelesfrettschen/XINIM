#include "../include/paging.h"
#include "const.hpp"
#include "type.hpp"

/* Simple 4-level page table allocator used only for illustration. */

PRIVATE struct pml4 kernel_pml4;
PRIVATE virt_addr64 next_kernel_va;

/*===========================================================================*
 *                              paging_init                                  *
 *===========================================================================*/
/**
 * @brief Initialize kernel paging structures.
 */
PUBLIC void paging_init(void) {
    int i;
    for (i = 0; i < PT_ENTRIES; i++)
        kernel_pml4.ptrs[i] = NIL_PTR;
    /* place kernel in higher half */
    next_kernel_va = U64_C(0xffff800000000000);
}

/*===========================================================================*
 *                              alloc_virtual                                *
 *===========================================================================*/
/**
 * @brief Allocate virtual kernel address space.
 *
 * @param bytes Size in bytes to allocate.
 * @param flags Allocation flags (unused).
 * @return Pointer to allocated virtual address space.
 */
PUBLIC void *alloc_virtual(u64_t bytes, int flags) {
    virt_addr64 va = next_kernel_va;
    unsigned long pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    next_kernel_va += pages * PAGE_SIZE_4K;
    (void)flags; /* protection flags not modelled */
    return (void *)va;
}

/*===========================================================================*
 *                              map_page                                     *
 *===========================================================================*/
/**
 * @brief Record a mapping from virtual to physical address.
 *
 * @param va    Virtual address to map.
 * @param pa    Physical address to map to.
 * @param flags Mapping attributes (unused).
 * @return OK on success.
 */
PUBLIC int map_page(virt_addr64 va, phys_addr64 pa, int flags) {
    /* Mapping is not actually implemented.  We only preserve bookkeeping. */
    unsigned idx4 = (va >> 39) & 0x1FF;
    if (!kernel_pml4.ptrs[idx4]) {
        kernel_pml4.ptrs[idx4] = (struct page_dir_ptr *)alloc_mem(
            (sizeof(struct page_dir_ptr) + CLICK_SIZE - 1) >> CLICK_SHIFT);
        memset(kernel_pml4.ptrs[idx4], 0, sizeof(struct page_dir_ptr));
    }
    /* Further levels would be allocated here in a complete system. */
    (void)pa;
    (void)flags;
    return OK;
}

#include "../include/paging.h"
#include "const.hpp"

/* User space virtual address allocator using 4-level page tables. */

static virt_addr64 next_user_va;

/*===========================================================================*
 *                              mm_paging_init                               *
 *===========================================================================*/
/* Initialize the user space paging allocator. */

void mm_paging_init() noexcept {
    /* start at low canonical address */
    next_user_va = U64_C(0x0000000000400000);
}

/*===========================================================================*
 *                              vm_alloc                                     *
 *===========================================================================*/
/* Allocate 'bytes' of virtual address space.
 * bytes: number of bytes requested.
 * flags: allocation flags (unused).
 */
void *vm_alloc(u64_t bytes, VmFlags flags) noexcept {
    virt_addr64 va = next_user_va;
    unsigned long pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    next_user_va += pages * PAGE_SIZE_4K;
    (void)flags;
    return (void *)va;
}

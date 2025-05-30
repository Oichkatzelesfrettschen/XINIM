/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/paging.hpp"
#include "const.hpp"

/* User space virtual address allocator using 4-level page tables. */

PRIVATE virt_addr64 next_user_va;

/*===========================================================================*
 *                              mm_paging_init                               *
 *===========================================================================*/
/* Initialize the user space paging allocator. */

PUBLIC void mm_paging_init(void) {
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
PUBLIC void *vm_alloc(u64_t bytes, int flags) {
    virt_addr64 va = next_user_va;
    unsigned long pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    next_user_va += pages * PAGE_SIZE_4K;
    (void)flags;
    return (void *)va;
}

#include "const.h"
#include "../include/paging.h"

/* User space virtual address allocator using 4-level page tables. */

PRIVATE virt_addr64 next_user_va;

PUBLIC void mm_paging_init(void)
{
    /* start at low canonical address */
    next_user_va = 0x0000000000400000ULL;
}

PUBLIC void *vm_alloc(unsigned long long bytes, int flags)
{
    virt_addr64 va = next_user_va;
    unsigned long pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    next_user_va += pages * PAGE_SIZE_4K;
    (void)flags;
    return (void *)va;
}

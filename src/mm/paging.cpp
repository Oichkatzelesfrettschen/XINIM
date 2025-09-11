#include "../include/paging.h"
#include "const.hpp"

/**
 * @file paging.cpp
 * @brief Simple user-space virtual address allocator built atop 4-level page
 * tables.
 *
 * The allocator dispenses sequential virtual address ranges from a monotonically
 * increasing cursor.  Actual page table structures are defined in
 * @ref paging.h but are not manipulated here.
 *
 * @ingroup memory
 */

/**
 * @brief Next free user-space virtual address.
 *
 * Allocation proceeds in page-sized quanta, so @ref next_user_va is always
 * aligned to ::PAGE_SIZE_4K bytes.
 *
 * @ingroup memory
 */
static virt_addr64 next_user_va;

/**
 * @brief Initialise the user space paging allocator.
 *
 * Sets @ref next_user_va to the lowest canonical user-space address.
 *
 * @ingroup memory
 */
void mm_paging_init() noexcept {
    /* start at low canonical address */
    next_user_va = U64_C(0x0000000000400000);
}

/**
 * @brief Allocate a range of virtual addresses.
 *
 * Ownership of the reserved range is returned to the caller.  The size is
 * rounded up to a multiple of ::PAGE_SIZE_4K and the base is page aligned.
 * The allocator performs no bookkeeping for freeing ranges.
 *
 * @param bytes Number of bytes requested.
 * @param flags Allocation flags (currently unused).
 *
 * @return Pointer to the base of the reserved range.
 *
 * @ingroup memory
 */
void *vm_alloc(u64_t bytes, VmFlags flags) noexcept {
    virt_addr64 va = next_user_va;
    unsigned long pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    next_user_va += pages * PAGE_SIZE_4K;
    (void)flags;
    return (void *)va;
}

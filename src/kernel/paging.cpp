#include "../../include/paging.hpp" // Corrected path and extension
#include "const.hpp"               // For PT_ENTRIES, CLICK_SIZE, CLICK_SHIFT, OK
#include "sys/type.hpp"           // For phys_addr64, virt_addr64 (though paging.hpp also defines them)
#include <cstdint>                  // For uint64_t
#include <cstddef>                  // For std::size_t, nullptr
#include <cstring>                  // For memset

// Assuming alloc_mem is declared elsewhere, e.g. from mm/alloc.cpp via a kernel header
// extern uint64_t alloc_mem(uint64_t clicks) noexcept; // phys_clicks -> uint64_t

/* Simple 4-level page table allocator used only for illustration. */

static struct pml4 kernel_pml4; // PRIVATE -> static
// virt_addr64 from paging.hpp is u64_t (uint64_t)
static uint64_t next_kernel_va;   // PRIVATE -> static

/*===========================================================================*
 *                              paging_init                                  *
 *===========================================================================*/
/**
 * @brief Initialize kernel paging structures.
 *
 * @pre Boot memory allocator is operational.
 * @post Kernel PML4 cleared and higher-half mapping base set.
 */
PUBLIC void paging_init() { // (void) -> (), noexcept
    size_t i;
    for (i = 0; i < PT_ENTRIES; i++)
        kernel_pml4.ptrs[i] = nullptr; // NIL_PTR -> nullptr
    /* place kernel in higher half */
    next_kernel_va = UINT64_C(0xffff800000000000); // U64_C -> UINT64_C for standard C++
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
 *
 * @pre ::paging_init has prepared @c next_kernel_va.
 * @post Reserved region is advanced by requested number of pages.
 */
// bytes is u64_t (uint64_t)
PUBLIC void *alloc_virtual(uint64_t bytes, int flags) {
    uint64_t va = next_kernel_va; // virt_addr64 -> uint64_t
    // bytes is uint64_t. PAGE_SIZE_4K from paging.hpp is std::size_t.
    // Result should be uint64_t if bytes can be large.
    uint64_t pages = (bytes + static_cast<uint64_t>(PAGE_SIZE_4K) - 1) / static_cast<uint64_t>(PAGE_SIZE_4K);
    next_kernel_va += pages * static_cast<uint64_t>(PAGE_SIZE_4K);
    (void)flags; /* protection flags not modelled */
    // Cast uint64_t address to void* via uintptr_t
    return reinterpret_cast<void*>(static_cast<uintptr_t>(va));
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
 *
 * @pre Corresponding PML4 entry has been allocated.
 * @post Lower level page tables remain unallocated (stub).
 * @todo Implement full 4-level page table population.
 */
// va, pa are virt_addr64, phys_addr64 (both uint64_t)
PUBLIC int map_page(uint64_t va, uint64_t pa, int flags) noexcept {
    /* Mapping is not actually implemented.  We only preserve bookkeeping. */
    unsigned int idx4 = static_cast<unsigned int>((va >> 39) & 0x1FF); // Result of bitops fits unsigned int
    if (!kernel_pml4.ptrs[idx4]) {
        // alloc_mem returns phys_clicks (uint64_t). This physical address (in clicks) needs conversion to bytes
        // and then to a pointer type. This assumes direct mapping or kernel virtual address = physical address.
        uint64_t p_addr_clicks = alloc_mem(
            (sizeof(struct page_dir_ptr) + static_cast<std::size_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT);
        // Convert physical click address to byte address for pointer cast
        uint64_t p_addr_bytes = p_addr_clicks << CLICK_SHIFT;
        kernel_pml4.ptrs[idx4] = reinterpret_cast<struct page_dir_ptr*>(static_cast<uintptr_t>(p_addr_bytes));

        memset(kernel_pml4.ptrs[idx4], 0, sizeof(struct page_dir_ptr));
    }
    /* Further levels would be allocated here in a complete system. */
    (void)pa;
    (void)flags;
    return OK;
}

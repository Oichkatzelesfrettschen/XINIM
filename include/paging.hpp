/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/**
 * @file
 * @brief Generic 4-level paging structures and kernel interfaces.
 */

#ifndef PAGING_H
#define PAGING_H

#include "../h/const.hpp"
#include "../h/type.hpp"
#include <cstddef> // for std::size_t

/* Generic 4-level paging structures for 48-bit virtual addresses. */

/** Size in bytes of a single 4 KiB page. */
inline constexpr std::size_t PAGE_SIZE_4K = 4096;

/** Number of entries in each level of the page table hierarchy. */
inline constexpr std::size_t PT_ENTRIES = 512;

/**
 * @brief Page table entry flags encoded as bit values.
 */
enum PT_Flag : unsigned long {
    PT_PRESENT = 0x001,  ///< Entry is valid.
    PT_WRITABLE = 0x002, ///< Memory region is writable.
    PT_USER = 0x004      ///< Accessible from user mode.
};

using phys_addr64 = u64_t;
using virt_addr64 = u64_t;

/**
 * @brief Single page table entry mapping a page to a physical address.
 */
struct pt_entry {
    phys_addr64 addr;   ///< Physical address referenced by the entry.
    unsigned long flags; ///< Attribute bits from ::PT_Flag.
};

/**
 * @brief Page table containing ::PT_ENTRIES entries.
 */
struct page_table {
    pt_entry entries[PT_ENTRIES]; ///< Array of page table entries.
};

/**
 * @brief Page directory referencing lower level tables.
 */
struct page_directory {
    page_table *tables[PT_ENTRIES]; ///< Pointers to page tables.
};

/**
 * @brief Directory-pointer level aggregating page directories.
 */
struct page_dir_ptr {
    page_directory *dirs[PT_ENTRIES]; ///< Pointers to page directories.
};

/**
 * @brief Top-level page map for 4-level paging.
 */
struct pml4 {
    page_dir_ptr *ptrs[PT_ENTRIES]; ///< Pointers to directory pointers.
};

/* Kernel interfaces */

/**
 * @brief Initialize kernel paging structures.
 */
void paging_init(void);

/**
 * @brief Allocate virtual kernel address space.
 *
 * @param bytes Number of bytes to allocate.
 * @param flags Allocation attributes.
 * @return Pointer to the allocated virtual region.
 */
void *alloc_virtual(u64_t bytes, int flags);

/**
 * @brief Record a mapping from virtual to physical address.
 *
 * @param va Virtual address to map.
 * @param pa Physical address to associate.
 * @param flags Mapping attributes.
 * @return ::OK on success or an error code.
 */
int map_page(virt_addr64 va, phys_addr64 pa, int flags);

#endif /* PAGING_H */

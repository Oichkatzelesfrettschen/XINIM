/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#ifndef PAGING_H
#define PAGING_H

#include "../h/const.hpp"
#include "../h/type.hpp"
#include <cstddef> // for std::size_t

/* Generic 4-level paging structures for 48-bit virtual addresses. */

// Size in bytes of a single 4 KiB page.
inline constexpr std::size_t PAGE_SIZE_4K = 4096;

// Number of entries in each level of the page table hierarchy.
inline constexpr std::size_t PT_ENTRIES = 512;

/* Page table entry flags */
// Page table entry flags encoded as bit values.
enum PT_Flag : unsigned long {
    PT_PRESENT = 0x001,  // entry is valid
    PT_WRITABLE = 0x002, // memory region is writable
    PT_USER = 0x004      // accessible from user mode
};

typedef u64_t phys_addr64;
typedef u64_t virt_addr64;

struct pt_entry {
    phys_addr64 addr;
    unsigned long flags;
};

struct page_table {
    struct pt_entry entries[PT_ENTRIES];
};

struct page_directory {
    struct page_table *tables[PT_ENTRIES];
};

struct page_dir_ptr {
    struct page_directory *dirs[PT_ENTRIES];
};

struct pml4 {
    struct page_dir_ptr *ptrs[PT_ENTRIES];
};

/* Kernel interfaces */
void paging_init(void);
void *alloc_virtual(u64_t bytes, int flags);
int map_page(virt_addr64 va, phys_addr64 pa, int flags);

#endif /* PAGING_H */

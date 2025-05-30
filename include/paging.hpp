/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#ifndef PAGING_H
#define PAGING_H

#include "../h/const.hpp"
#include "../h/type.hpp"

/* Generic 4-level paging structures for 48-bit virtual addresses. */

#define PAGE_SIZE_4K 4096
#define PT_ENTRIES 512

/* Page table entry flags */
#define PT_PRESENT 0x001
#define PT_WRITABLE 0x002
#define PT_USER 0x004

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

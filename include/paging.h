#ifndef PAGING_H
#define PAGING_H

#include "../h/const.h"
#include "../h/type.h"

/* Generic 4-level paging structures for 48-bit virtual addresses. */

#define PAGE_SIZE_4K       4096
#define PT_ENTRIES         512

/* Page table entry flags */
#define PT_PRESENT   0x001
#define PT_WRITABLE  0x002
#define PT_USER      0x004

typedef unsigned long long phys_addr64;
typedef unsigned long long virt_addr64;

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
void *alloc_virtual(unsigned long long bytes, int flags);
int map_page(virt_addr64 va, phys_addr64 pa, int flags);

#endif /* PAGING_H */

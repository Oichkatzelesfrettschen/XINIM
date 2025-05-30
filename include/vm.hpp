/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#ifndef VM_H
#define VM_H

#include "../h/const.hpp"
#include "paging.hpp"

/* Flags for virtual memory regions */
#define VM_READ 0x1
#define VM_WRITE 0x2
#define VM_EXEC 0x4
#define VM_PRIVATE 0x8
#define VM_SHARED 0x10
#define VM_ANON 0x20

#define VM_MAX_AREAS 16

struct vm_area {
    virt_addr64 start; /* inclusive start address */
    virt_addr64 end;   /* exclusive end address */
    int flags;         /* protection flags */
};

struct vm_proc {
    struct vm_area areas[VM_MAX_AREAS];
    int area_count;
};

void vm_init(void);
void *vm_alloc(u64_t bytes, int flags);
void vm_handle_fault(int proc, virt_addr64 addr);
int vm_fork(int parent, int child);
void *vm_mmap(int proc, void *addr, u64_t length, int flags);

#endif /* VM_H */

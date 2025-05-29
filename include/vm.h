#ifndef VM_H
#define VM_H

#include "paging.h"
#include "../h/const.h"

/* Flags for virtual memory regions */
#define VM_READ     0x1
#define VM_WRITE    0x2
#define VM_EXEC     0x4
#define VM_PRIVATE  0x8
#define VM_SHARED   0x10
#define VM_ANON     0x20

#define VM_MAX_AREAS   16

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
void *vm_alloc(unsigned long long bytes, int flags);
void vm_handle_fault(int proc, virt_addr64 addr);
int vm_fork(int parent, int child);
void *vm_mmap(int proc, void *addr, unsigned long long length, int flags);

#endif /* VM_H */

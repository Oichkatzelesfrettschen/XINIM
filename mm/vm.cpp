#include "../include/vm.h"
#include "const.hpp"
#include "mproc.hpp"

/*
 * Simple virtual memory subsystem used for demonstration.  Each process gets
 * a set of virtual memory areas stored in vm_proc_table.  Actual page table
 * management is omitted and only bookkeeping is performed.
 */

/* Per-process virtual memory information. */
PRIVATE struct vm_proc vm_proc_table[NR_PROCS];

/* State for a very small pseudo random generator used for ASLR. */
PRIVATE unsigned long rng_state = 1;

/*===========================================================================*
 *                              next_rand                                   *
 *===========================================================================*/
/* Generate a pseudo random number.
 * no parameters.
 */
PRIVATE unsigned long next_rand(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

/*===========================================================================*
 *                              vm_init                                      *
 *===========================================================================*/
/* Initialise the virtual memory subsystem.
 * no parameters.
 */
PUBLIC void vm_init(void) {
    int i;
    for (i = 0; i < NR_PROCS; i++) {
        vm_proc_table[i].area_count = 0;
    }
    rng_state = 1;
}

/*===========================================================================*
 *                              vm_alloc                                     *
 *===========================================================================*/
/* Allocate virtual address space with ASLR.
 * bytes: size in bytes to allocate.
 * flags: protection flags (unused).
 */
PUBLIC void *vm_alloc(u64_t bytes, int flags) {
    virt_addr64 base;
    unsigned long pages;

    /* randomise base for a little ASLR */
    base = U64_C(0x0000000010000000) + (next_rand() & 0xFFFFF000);
    pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    (void)flags; /* protection flags unused in this stub */
    return (void *)(base + pages * PAGE_SIZE_4K);
}

/*===========================================================================*
 *                              vm_handle_fault                              *
 *===========================================================================*/
/* Handle a page fault by recording the accessed region.
 * proc: process index causing the fault.
 * addr: faulting virtual address.
 */
PUBLIC void vm_handle_fault(int proc, virt_addr64 addr) {
    /* This routine would allocate a page frame and map it.  Here it is
     * recorded only for bookkeeping.
     */
    struct vm_proc *p = &vm_proc_table[proc];
    if (p->area_count < VM_MAX_AREAS) {
        struct vm_area *a = &p->areas[p->area_count++];
        a->start = addr & ~(PAGE_SIZE_4K - 1);
        a->end = a->start + PAGE_SIZE_4K;
        a->flags = VM_READ | VM_WRITE | VM_PRIVATE;
    }
}

/*===========================================================================*
 *                              vm_fork                                      *
 *===========================================================================*/
/* Duplicate parent's memory bookkeeping for a child process.
 * parent: index of the parent process.
 * child: index of the child process.
 */
PUBLIC int vm_fork(int parent, int child) {
    vm_proc_table[child] = vm_proc_table[parent];
    return OK;
}

/*===========================================================================*
 *                              vm_mmap                                      *
 *===========================================================================*/
/* Map a region of memory into a process.
 * proc:   process index to map into.
 * addr:   desired base address or NULL.
 * length: length of mapping in bytes.
 * flags:  mapping flags.
 */
PUBLIC void *vm_mmap(int proc, void *addr, u64_t length, int flags) {
    struct vm_proc *p = &vm_proc_table[proc];
    virt_addr64 base = (virt_addr64)addr;

    if (!base) {
        base = (virt_addr64)vm_alloc(length, flags);
    }

    if (p->area_count < VM_MAX_AREAS) {
        struct vm_area *a = &p->areas[p->area_count++];
        a->start = base;
        a->end = base + length;
        a->flags = flags;
    }

    return (void *)base;
}

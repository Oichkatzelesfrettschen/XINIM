#include "../include/vm.hpp"
#include "const.hpp"
#include "mproc.hpp"
#include <vector>

/**
 * @file vm.cpp
 * @brief Minimal virtual memory bookkeeping for processes.
 *
 * The module models per-process address spaces using the @ref vm_proc_table
 * vector.  Each entry tracks up to ::VM_MAX_AREAS regions via ::vm_area
 * descriptors but does not manage actual hardware page tables.
 *
 * @ingroup memory
 */

/**
 * @brief Per-process virtual memory information.
 *
 * Each element owns an array of ::vm_area descriptors describing regions
 * reserved within the process.  The table retains ownership of these
 * descriptors; callers manipulate only addresses.
 *
 * @ingroup memory
 */
static std::vector<vm_proc> vm_proc_table(NR_PROCS);

/* State for a very small pseudo random generator used for ASLR. */
static unsigned long rng_state = 1;

/*===========================================================================*
 *                              next_rand                                   *
 *===========================================================================*/
/* Generate a pseudo random number.
 * no parameters.
 */
/**
 * @brief Generate a pseudo random number used for address randomisation.
 *
 * @return Next pseudo random value.
 *
 * @ingroup memory
 */
static unsigned long next_rand() {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

/*===========================================================================*
 *                              vm_init                                      *
 *===========================================================================*/
/* Initialise the virtual memory subsystem.
 * no parameters.
 */
/**
 * @brief Initialise the virtual memory subsystem.
 *
 * Clears all per-process area records and resets the random generator.
 *
 * @ingroup memory
 */
void vm_init() noexcept {
    for (auto &p : vm_proc_table) {
        p.area_count = 0;
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
/**
 * @brief Allocate a region of virtual memory with simple ASLR.
 *
 * The returned base is aligned to ::PAGE_SIZE_4K and chosen with a trivial
 * pseudo random offset to mimic address-space layout randomisation.  Ownership
 * of the virtual region is passed to the caller; no freeing is supported.
 *
 * @param bytes Number of bytes to allocate.
 * @param flags Protection flags (currently unused).
 *
 * @return Base address of the allocated region.
 *
 * @ingroup memory
 */
void *vm_alloc(u64_t bytes, VmFlags flags) noexcept {
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
/**
 * @brief Record a page fault within a process.
 *
 * The faulting address is rounded down to a page boundary and stored as a new
 * ::vm_area if room is available.
 *
 * @param proc Index of the faulting process.
 * @param addr Faulting virtual address.
 *
 * @ingroup memory
 */
void vm_handle_fault(int proc, virt_addr64 addr) noexcept {
    /* This routine would allocate a page frame and map it.  Here it is
     * recorded only for bookkeeping.
     */
    auto &p = vm_proc_table[proc];
    if (p.area_count < VM_MAX_AREAS) {
        auto &a = p.areas[p.area_count++];
        a.start = addr & ~(PAGE_SIZE_4K - 1);
        a.end = a.start + PAGE_SIZE_4K;
        a.flags = VmFlags::VM_READ | VmFlags::VM_WRITE | VmFlags::VM_PRIVATE;
    }
}

/*===========================================================================*
 *                              vm_fork                                      *
 *===========================================================================*/
/**
 * @brief Duplicate a parent's virtual memory bookkeeping for a child.
 *
 * Only metadata is copied; physical frames and page tables remain distinct.
 *
 * @param parent Index of the parent process.
 * @param child  Index of the child process.
 *
 * @return ::OK on success.
 *
 * @ingroup memory
 */
int vm_fork(int parent, int child) noexcept {
    vm_proc_table[child] = vm_proc_table[parent];
    return OK;
}

/*===========================================================================*
 *                              vm_mmap                                      *
 *===========================================================================*/
/**
 * @brief Map a region of memory into a process.
 *
 * Updates the bookkeeping in @ref vm_proc_table by appending a new ::vm_area
 * descriptor.  The base is page aligned, and ownership of the descriptor
 * resides with the process record.
 *
 * @param proc   Process index to map into.
 * @param addr   Desired base address or nullptr for automatic placement.
 * @param length Length of the mapping in bytes.
 * @param flags  Mapping flags.
 *
 * @return Base address of the mapped region.
 *
 * @ingroup memory
 */
void *vm_mmap(int proc, void *addr, u64_t length, VmFlags flags) noexcept {
    auto &p = vm_proc_table[proc];
    virt_addr64 base = (virt_addr64)addr;

    if (!base) {
        base = (virt_addr64)vm_alloc(length, flags);
    }

    if (p.area_count < VM_MAX_AREAS) {
        auto &a = p.areas[p.area_count++];
        a.start = base;
        a.end = base + length;
        a.flags = flags;
    }

    return (void *)base;
}

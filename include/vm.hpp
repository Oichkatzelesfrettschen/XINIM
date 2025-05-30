/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#ifndef VM_H
#define VM_H

#include "../h/const.h"
#include "paging.hpp"

/* Flags for virtual memory regions. Converted from macros to a typesafe
 * enumeration so that the values can be combined with the usual bitwise
 * operators. */
enum class VmFlags : int {
    Read = 0x1,
    Write = 0x2,
    Exec = 0x4,
    Private = 0x8,
    Shared = 0x10,
    Anon = 0x20
};

/* Enable bitwise operations on VmFlags. */
inline constexpr VmFlags operator|(VmFlags l, VmFlags r) {
    return static_cast<VmFlags>(static_cast<int>(l) | static_cast<int>(r));
}
inline constexpr VmFlags operator&(VmFlags l, VmFlags r) {
    return static_cast<VmFlags>(static_cast<int>(l) & static_cast<int>(r));
}

/* Maximum number of areas that can be tracked for a process. */
inline constexpr int VM_MAX_AREAS = 16;

/* Describes a contiguous range of virtual addresses with associated
 * protection flags. */
struct vm_area {
    virt_addr64 start; /* inclusive start address */
    virt_addr64 end;   /* exclusive end address */
    VmFlags flags;     /* protection flags */
};

struct vm_proc {
    struct vm_area areas[VM_MAX_AREAS];
    int area_count;
};

void vm_init(void);
void *vm_alloc(u64_t bytes, VmFlags flags);
void vm_handle_fault(int proc, virt_addr64 addr);
int vm_fork(int parent, int child);
void *vm_mmap(int proc, void *addr, u64_t length, VmFlags flags);

#endif /* VM_H */

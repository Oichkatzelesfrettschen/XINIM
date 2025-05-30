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
/* Enumerates flags describing permissions and properties for a
 * virtual memory region.  The values are kept compatible with the
 * original macro definitions.
 */
enum class VmFlags : int {
    VM_READ = 0x1,    /* region is readable */
    VM_WRITE = 0x2,   /* region is writable */
    VM_EXEC = 0x4,    /* region is executable */
    VM_PRIVATE = 0x8, /* region is private */
    VM_SHARED = 0x10, /* region is shared */
    VM_ANON = 0x20    /* region is anonymous */
};

/* Enable bitwise operations for VmFlags. */
inline constexpr VmFlags operator|(VmFlags lhs, VmFlags rhs) {
    return static_cast<VmFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
inline constexpr VmFlags operator&(VmFlags lhs, VmFlags rhs) {
    return static_cast<VmFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
inline VmFlags &operator|=(VmFlags &lhs, VmFlags rhs) {
    lhs = lhs | rhs;
    return lhs;
}
inline VmFlags &operator&=(VmFlags &lhs, VmFlags rhs) {
    lhs = lhs & rhs;
    return lhs;
}

/* Maximum number of areas that can be tracked for a process. */
inline constexpr int VM_MAX_AREAS = 16;

/* Describes a contiguous range of virtual addresses with associated
 * protection flags. */
/* Describes a contiguous virtual memory area owned by a process. */
struct vm_area {
    virt_addr64 start; /* inclusive start address */
    virt_addr64 end;   /* exclusive end address */
    VmFlags flags;     /* protection flags */
};

struct vm_proc {
    struct vm_area areas[VM_MAX_AREAS]; /* list of owned areas */
    int area_count;                     /* number of valid entries */
};

void vm_init(void);
void *vm_alloc(u64_t bytes, VmFlags flags);
void vm_handle_fault(int proc, virt_addr64 addr);
int vm_fork(int parent, int child);
void *vm_mmap(int proc, void *addr, u64_t length, VmFlags flags);

#endif /* VM_H */

# Week 8 Phase 4: Syscall Infrastructure - Implementation Plan

**Date**: November 17, 2025
**Status**: 🚧 IN PROGRESS
**Branch**: `claude/audit-translate-repo-018NNYQuW6XJrQJL2LLUPS45`
**Depends On**: Week 8 Phase 3 (Ring 3 Transition) ✅

---

## Executive Summary

**Goal**: Implement fast syscall mechanism (syscall/sysret instructions) to enable Ring 3 servers to invoke kernel services.

**Why This Matters**:
- **Performance**: syscall/sysret is much faster than int 0x80 (2x-3x faster)
- **Modern**: All modern x86_64 CPUs support it
- **Required**: Ring 3 processes need a way to call kernel functions
- **IPC Integration**: Syscalls will delegate to Lattice IPC for server communication

**Estimated Time**: 4-6 hours

---

## Current State Analysis

### What Works (Phase 3)
- ✅ Servers run in Ring 3 (user mode)
- ✅ GDT configured with Ring 0 and Ring 3 segments
- ✅ TSS configured for kernel stack switching
- ✅ Timer interrupts work in Ring 3
- ✅ Context switching preserves privilege levels

### What's Missing (No Syscall Mechanism)
- ❌ Ring 3 processes cannot call kernel functions
- ❌ No syscall handler
- ❌ No syscall dispatch table
- ❌ No userspace syscall wrappers
- ❌ Servers cannot use POSIX functions (write, read, etc.)

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Week 8 Phase 3 (Current)                 │
├─────────────────────────────────────────────────────────────┤
│  Ring 3:  [VFS] [Proc Mgr] [Memory Mgr]                    │
│           (no way to call kernel!)                          │
│                                                             │
│  Ring 0:  [Kernel: Scheduler, IPC, Drivers]                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                Week 8 Phase 4 (Goal)                        │
├─────────────────────────────────────────────────────────────┤
│  Ring 3:  [VFS] [Proc Mgr] [Memory Mgr]                    │
│             ↓ syscall (RAX=syscall_number)                 │
│  Ring 0:  [Syscall Handler] → Dispatch Table               │
│             ↓                                               │
│           [Kernel Functions] → Lattice IPC → Servers       │
│             ↓ sysret                                        │
│  Ring 3:  Return to caller                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## Technical Requirements

### 1. x86_64 syscall/sysret Instructions

**syscall** (Ring 3 → Ring 0):
1. Saves user RIP to RCX
2. Saves RFLAGS to R11
3. Loads kernel CS from IA32_STAR MSR (bits 47:32)
4. Loads kernel RIP from IA32_LSTAR MSR
5. Clears RFLAGS bits specified in IA32_FMASK MSR
6. Switches to Ring 0
7. Jumps to kernel syscall handler

**sysret** (Ring 0 → Ring 3):
1. Loads user CS from IA32_STAR MSR (bits 63:48 + 16)
2. Loads user RIP from RCX
3. Loads RFLAGS from R11
4. Switches to Ring 3
5. Jumps back to user code

**No automatic stack switch!** We must manually:
- Save user RSP
- Load kernel RSP from TSS or per-CPU variable
- Switch stacks in syscall handler

### 2. Model Specific Registers (MSRs)

**IA32_EFER** (MSR 0xC0000080):
- Bit 0: SCE (System Call Extensions) - must be set to enable syscall/sysret

**IA32_STAR** (MSR 0xC0000081):
```
Bits 63:48: User CS base (user code = base + 16, user data = base + 8)
Bits 47:32: Kernel CS base (kernel code = base, kernel SS = base + 8)
Bits 31:0:  Reserved
```

**IA32_LSTAR** (MSR 0xC0000082):
- 64-bit address of syscall handler

**IA32_FMASK** (MSR 0xC0000084):
- RFLAGS bits to clear on syscall (typically clear IF to disable interrupts)

### 3. Calling Convention

**System V AMD64 ABI** (used by Linux, BSD, macOS):

**Syscall Number**: RAX
**Arguments** (up to 6):
1. RDI
2. RSI
3. RDX
4. R10 (not RCX! RCX is overwritten by syscall)
5. R8
6. R9

**Return Value**: RAX (or RDX:RAX for 128-bit)

**Preserved Registers**: RBX, RBP, R12-R15
**Clobbered**: RAX, RCX, R11, and all argument registers

### 4. Syscall Number Space

XINIM will use a custom syscall numbering scheme aligned with POSIX.1-2017:

**Categories**:
- 0-99: Process management (exit, fork, exec, wait, getpid, etc.)
- 100-199: File I/O (open, read, write, close, lseek, etc.)
- 200-299: Memory management (mmap, munmap, brk, etc.)
- 300-399: IPC (pipe, msgget, shmat, etc.)
- 400-499: Networking (socket, bind, listen, accept, etc.)
- 500-599: Time/signals (alarm, signal, kill, etc.)

**Week 8 Phase 4 Syscalls** (minimal set):
- 0: exit
- 1: write
- 39: getpid
- 60: exit (duplicate for compatibility)

---

## Implementation Plan

### Phase 4.1: MSR Setup and Initialization

**File**: `src/kernel/arch/x86_64/syscall_init.cpp` (NEW)

**Tasks**:
1. Define MSR numbers and bit masks
2. Implement `rdmsr()` and `wrmsr()` functions
3. Implement `initialize_syscall()` function:
   - Enable SCE bit in IA32_EFER
   - Set IA32_STAR with kernel/user CS
   - Set IA32_LSTAR with syscall handler address
   - Set IA32_FMASK to clear IF (disable interrupts in handler)

**Code Structure**:
```cpp
namespace xinim::kernel {

// MSR numbers
constexpr uint32_t MSR_EFER   = 0xC0000080;
constexpr uint32_t MSR_STAR   = 0xC0000081;
constexpr uint32_t MSR_LSTAR  = 0xC0000082;
constexpr uint32_t MSR_FMASK  = 0xC0000084;

// EFER bits
constexpr uint64_t EFER_SCE = (1ULL << 0);  // System Call Extensions

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

void initialize_syscall();

} // namespace xinim::kernel
```

**MSR Values**:
```cpp
// IA32_STAR format:
// Bits 63:48 = User CS base (0x1B - 16 = 0x0B, shifted left 48)
// Bits 47:32 = Kernel CS base (0x08, shifted left 32)
uint64_t star = 0;
star |= (0x0BULL << 48);  // User CS base
star |= (0x08ULL << 32);  // Kernel CS base
wrmsr(MSR_STAR, star);

// IA32_LSTAR = address of syscall_handler
wrmsr(MSR_LSTAR, (uint64_t)&syscall_handler);

// IA32_FMASK = clear IF (bit 9) on syscall
wrmsr(MSR_FMASK, (1 << 9));
```

---

### Phase 4.2: Syscall Handler Assembly

**File**: `src/arch/x86_64/syscall_handler.S` (NEW)

**Tasks**:
1. Save user context (RAX through R15)
2. Switch to kernel stack (from TSS or per-CPU)
3. Call C++ dispatch function
4. Restore user context
5. Return via sysret

**Critical**: Must manually switch stacks!

**Assembly Code**:
```asm
.section .text
.code64

.global syscall_handler
.type syscall_handler, @function
.align 16
syscall_handler:
    # At entry:
    # - RCX = user RIP (saved by CPU)
    # - R11 = user RFLAGS (saved by CPU)
    # - RAX = syscall number
    # - RDI, RSI, RDX, R10, R8, R9 = arguments
    # - We are in Ring 0 with user stack!

    # CRITICAL: Switch to kernel stack
    # Option 1: Use swapgs + per-CPU kernel stack
    # Option 2: Read from TSS (slower but simpler for Week 8)

    swapgs  # GS now points to per-CPU data

    # Save user RSP to per-CPU area
    mov qword ptr gs:[0], rsp

    # Load kernel RSP from per-CPU area
    mov rsp, qword ptr gs:[8]

    # Save user registers on kernel stack
    push r11          # RFLAGS
    push rcx          # RIP
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    # Arguments for syscall_dispatch:
    # RDI = syscall number (from RAX)
    # RSI = arg1 (from RDI)
    # RDX = arg2 (from RSI)
    # RCX = arg3 (from RDX)
    # R8  = arg4 (from R10)
    # R9  = arg5 (from R8)
    # Stack: arg6 (from R9)

    mov rdi, rax      # syscall number
    mov rsi, rdi      # arg1
    mov rdx, rsi      # arg2
    mov rcx, rdx      # arg3
    mov r8, r10       # arg4
    mov r9, r8        # arg5
    push r9           # arg6 on stack

    # Call C++ dispatch function
    call syscall_dispatch

    # RAX now contains return value

    # Restore user registers (skip RAX, will have return value)
    add rsp, 8        # Skip arg6
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    pop rcx           # User RIP
    pop r11           # User RFLAGS

    # Restore user RSP
    mov rsp, qword ptr gs:[0]

    swapgs  # Restore user GS

    # Return to user mode
    sysretq

.size syscall_handler, . - syscall_handler
```

**Simplified Version** (no per-CPU for Week 8):
```asm
syscall_handler:
    # Save registers on user stack (temporary)
    push rbx
    push rbp
    # ... etc

    # Call dispatch
    call syscall_dispatch

    # Restore registers
    pop rbp
    pop rbx
    # ... etc

    sysretq
```

---

### Phase 4.3: Syscall Dispatch Table

**File**: `src/kernel/syscall_table.cpp` (NEW)

**Tasks**:
1. Define syscall numbers (enum or constexpr)
2. Define function pointer type for syscalls
3. Create dispatch table array
4. Implement `syscall_dispatch()` function

**Code Structure**:
```cpp
namespace xinim::kernel {

// Syscall numbers
enum class SyscallNumber : uint64_t {
    EXIT   = 0,
    WRITE  = 1,
    GETPID = 39,
    // More syscalls...
};

constexpr size_t MAX_SYSCALLS = 512;

// Syscall function signature
// Returns: int64_t (return value or error code)
// Arguments: up to 6 uint64_t values
using SyscallHandler = int64_t (*)(uint64_t arg1, uint64_t arg2,
                                     uint64_t arg3, uint64_t arg4,
                                     uint64_t arg5, uint64_t arg6);

// Forward declarations
int64_t sys_exit(uint64_t status, ...);
int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count, ...);
int64_t sys_getpid(...);

// Syscall table
static SyscallHandler g_syscall_table[MAX_SYSCALLS] = {
    [0] = sys_exit,
    [1] = sys_write,
    [39] = sys_getpid,
    // Initialize rest to nullptr
};

} // namespace xinim::kernel
```

**Dispatch Function**:
```cpp
extern "C" int64_t syscall_dispatch(uint64_t syscall_num,
                                     uint64_t arg1, uint64_t arg2,
                                     uint64_t arg3, uint64_t arg4,
                                     uint64_t arg5, uint64_t arg6) {
    using namespace xinim::kernel;

    // Validate syscall number
    if (syscall_num >= MAX_SYSCALLS) {
        return -ENOSYS;  // Function not implemented
    }

    SyscallHandler handler = g_syscall_table[syscall_num];
    if (!handler) {
        return -ENOSYS;
    }

    // Call handler
    return handler(arg1, arg2, arg3, arg4, arg5, arg6);
}
```

---

### Phase 4.4: Syscall Implementations

**File**: `src/kernel/syscalls/basic.cpp` (NEW)

**Tasks**:
1. Implement `sys_exit()`
2. Implement `sys_write()`
3. Implement `sys_getpid()`

**sys_exit**:
```cpp
int64_t sys_exit(uint64_t status, ...) {
    // Get current process
    ProcessControlBlock* current = get_current_process();

    early_serial.write("[SYSCALL] exit(");
    early_serial.write_hex(status);
    early_serial.write(")\n");

    // Mark process as ZOMBIE
    current->state = ProcessState::ZOMBIE;

    // TODO: Free resources, notify parent

    // Yield to next process
    schedule();

    // Never returns
    for(;;) {}
}
```

**sys_write**:
```cpp
int64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t count, ...) {
    // Validate arguments
    if (fd != 1 && fd != 2) {
        return -EBADF;  // Bad file descriptor
    }

    const char* buf = reinterpret_cast<const char*>(buf_addr);

    // Week 8: Simple implementation - write to serial
    for (uint64_t i = 0; i < count; i++) {
        early_serial.write_char(buf[i]);
    }

    return (int64_t)count;  // Return bytes written
}
```

**sys_getpid**:
```cpp
int64_t sys_getpid(...) {
    ProcessControlBlock* current = get_current_process();
    return (int64_t)current->pid;
}
```

---

### Phase 4.5: Userspace Syscall Wrappers

**File**: `src/userland/libc/syscall.h` (NEW)

**Tasks**:
1. Define syscall numbers
2. Create inline assembly syscall wrappers
3. Provide C function interfaces

**Code**:
```c
#ifndef XINIM_SYSCALL_H
#define XINIM_SYSCALL_H

#include <stdint.h>
#include <sys/types.h>

// Syscall numbers
#define SYS_exit   0
#define SYS_write  1
#define SYS_getpid 39

// Generic syscall with up to 6 arguments
static inline int64_t syscall6(uint64_t num,
                                uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4,
                                uint64_t arg5, uint64_t arg6) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8  __asm__("r8")  = arg5;
    register uint64_t r9  __asm__("r9")  = arg6;

    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );

    return ret;
}

// Convenience wrappers
static inline void _exit(int status) {
    syscall6(SYS_exit, status, 0, 0, 0, 0, 0);
}

static inline ssize_t write(int fd, const void* buf, size_t count) {
    return syscall6(SYS_write, fd, (uint64_t)buf, count, 0, 0, 0);
}

static inline pid_t getpid(void) {
    return (pid_t)syscall6(SYS_getpid, 0, 0, 0, 0, 0, 0);
}

#endif /* XINIM_SYSCALL_H */
```

---

### Phase 4.6: Per-CPU Kernel Stack (Simplified)

**File**: `src/kernel/arch/x86_64/percpu.cpp` (NEW)

**Tasks**:
1. Define per-CPU structure
2. Allocate per-CPU kernel stack
3. Initialize GS_BASE MSR to point to per-CPU data

**Structure**:
```cpp
namespace xinim::kernel {

struct PerCpuData {
    uint64_t user_rsp;           // Offset 0: Saved user RSP
    uint64_t kernel_rsp;         // Offset 8: Kernel RSP for syscall
    uint64_t current_process;    // Offset 16: Pointer to current PCB
    uint8_t kernel_stack[8192];  // Kernel stack (8 KB)
} __attribute__((aligned(64)));

static PerCpuData g_percpu_data;

void initialize_percpu() {
    // Set kernel RSP to top of kernel stack
    g_percpu_data.kernel_rsp =
        (uint64_t)&g_percpu_data.kernel_stack[8192];

    // Write GS_BASE MSR to point to per-CPU data
    wrmsr(MSR_GS_BASE, (uint64_t)&g_percpu_data);
}

} // namespace xinim::kernel
```

---

### Phase 4.7: Integration and Testing

**File**: `src/servers/vfs_server/main.cpp` (MODIFY)

**Test Code**:
```cpp
#include "syscall.h"

extern "C" void vfs_server_main() {
    // Test syscalls
    pid_t my_pid = getpid();

    char msg[64];
    snprintf(msg, sizeof(msg), "VFS Server: PID=%d\n", my_pid);
    write(1, msg, strlen(msg));

    // Main server loop
    for(;;) {
        __asm__ volatile ("hlt");
    }

    _exit(0);
}
```

---

## File Changes Summary

### New Files (8)
1. `docs/WEEK8_PHASE4_SYSCALLS_PLAN.md` (~800 LOC)
2. `src/kernel/arch/x86_64/syscall_init.cpp` (~150 LOC)
3. `src/kernel/arch/x86_64/syscall_init.hpp` (~50 LOC)
4. `src/arch/x86_64/syscall_handler.S` (~200 LOC)
5. `src/kernel/syscall_table.cpp` (~100 LOC)
6. `src/kernel/syscall_table.hpp` (~80 LOC)
7. `src/kernel/syscalls/basic.cpp` (~150 LOC)
8. `src/userland/libc/syscall.h` (~100 LOC)

### Modified Files (3)
1. `src/kernel/main.cpp` (+5, 0) - Initialize syscalls
2. `src/servers/vfs_server/main.cpp` (+10, -5) - Test syscalls
3. `xmake.lua` (+3, 0) - Add new files

### Total Estimated LOC
- **New**: ~1,630 LOC
- **Modified**: +15, -5 = +10 net
- **Total**: ~1,640 LOC

---

## Expected Boot Sequence (Phase 4)

```
1. Initialize GDT, TSS
2. Initialize syscalls:
   → Enable SCE in IA32_EFER
   → Set IA32_STAR (kernel/user CS)
   → Set IA32_LSTAR (syscall handler address)
   → Set IA32_FMASK (clear IF)
3. Initialize scheduler
4. Spawn VFS Server (Ring 3)
5. VFS server starts:
   → Calls getpid() (syscall 39)
   → CPU executes syscall instruction
   → Switches to Ring 0
   → Jumps to syscall_handler
   → Dispatch to sys_getpid()
   → Returns PID 2
   → sysret back to Ring 3
   → VFS continues execution
6. VFS calls write():
   → syscall instruction
   → Dispatch to sys_write()
   → Writes to serial console
   → Returns bytes written
   → sysret to Ring 3
7. System running with full syscall support!
```

---

## Success Criteria

Phase 4 is **COMPLETE** when:

- [ ] MSR setup code implemented (SCE, STAR, LSTAR, FMASK)
- [ ] Syscall handler assembly implemented
- [ ] Syscall dispatch table created
- [ ] Basic syscalls implemented (exit, write, getpid)
- [ ] Userspace syscall wrappers created
- [ ] Servers can call syscalls from Ring 3
- [ ] Test shows successful syscall execution
- [ ] All code compiles without errors
- [ ] Documentation complete

---

## Timeline

**Total**: 4-6 hours

- **Hour 1**: MSR setup and syscall initialization
- **Hour 2**: Syscall handler assembly
- **Hour 3**: Dispatch table and basic syscalls
- **Hour 4**: Userspace wrappers and testing
- **Hour 5**: Integration and debugging
- **Hour 6**: Documentation and commit

---

**Status**: 🚧 Planning Complete, Ready to Execute!

**Next**: Begin implementation with MSR setup 🚀

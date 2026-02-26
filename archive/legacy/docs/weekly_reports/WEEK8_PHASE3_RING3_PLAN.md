# Week 8 Phase 3: Ring 3 Transition - Implementation Plan

**Date**: November 17, 2025
**Status**: 🚧 IN PROGRESS
**Branch**: `claude/audit-translate-repo-018NNYQuW6XJrQJL2LLUPS45`
**Depends On**: Week 8 Phase 2 (Preemptive Scheduling) ✅

---

## Executive Summary

**Goal**: Transition system servers from Ring 0 (kernel mode) to Ring 3 (user mode) for true privilege separation.

**Why This Matters**:
- **Security**: Servers can't corrupt kernel memory
- **Isolation**: Server bugs can't crash the kernel
- **POSIX Compliance**: True userspace/kernel separation
- **Microkernel Architecture**: Kernel is minimal, servers are isolated

**Estimated Time**: 4-6 hours

---

## Current State Analysis

### What Works (Phase 2)
- ✅ Preemptive scheduling with timer interrupts
- ✅ Context switching between processes
- ✅ Three servers spawned: VFS (PID 2), Process Mgr (PID 3), Memory Mgr (PID 4)
- ✅ Scheduler picks next process and switches context

### What's Missing (Ring 0 Limitation)
- ❌ All servers run in **Ring 0** (kernel mode)
- ❌ No privilege separation - servers can access kernel memory
- ❌ No syscall mechanism - servers can call kernel functions directly
- ❌ No user/kernel stack separation

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Week 8 Phase 2 (Current)                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Ring 0 (Kernel Mode):                                     │
│  ┌──────────────┬──────────────┬──────────────┐            │
│  │  VFS Server  │  Proc Mgr    │  Memory Mgr  │            │
│  │  (PID 2)     │  (PID 3)     │  (PID 4)     │            │
│  └──────────────┴──────────────┴──────────────┘            │
│  ┌────────────────────────────────────────────┐            │
│  │           Kernel (Scheduler, IPC)          │            │
│  └────────────────────────────────────────────┘            │
│                                                             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                Week 8 Phase 3 (Goal)                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Ring 3 (User Mode):                                       │
│  ┌──────────────┬──────────────┬──────────────┐            │
│  │  VFS Server  │  Proc Mgr    │  Memory Mgr  │            │
│  │  (PID 2)     │  (PID 3)     │  (PID 4)     │            │
│  └──────────────┴──────────────┴──────────────┘            │
│                        ↕ syscall                           │
│  Ring 0 (Kernel Mode):                                     │
│  ┌────────────────────────────────────────────┐            │
│  │           Kernel (Scheduler, IPC)          │            │
│  └────────────────────────────────────────────┘            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Technical Requirements

### 1. x86_64 Privilege Levels

**Rings**:
- Ring 0: Kernel mode (highest privilege)
- Ring 1: Unused on most modern OSes
- Ring 2: Unused on most modern OSes
- Ring 3: User mode (lowest privilege)

**Segment Selectors**:
```
Bits 0-1: RPL (Requested Privilege Level)
  00 = Ring 0
  11 = Ring 3

Bit 2: Table Indicator
  0 = GDT
  1 = LDT

Bits 3-15: Index into GDT/LDT

Example:
  0x08 = 0b00001000 = GDT entry 1, Ring 0 (kernel code)
  0x10 = 0b00010000 = GDT entry 2, Ring 0 (kernel data)
  0x1B = 0b00011011 = GDT entry 3, Ring 3 (user code)
  0x23 = 0b00100011 = GDT entry 4, Ring 3 (user data)
```

### 2. Context Switching Ring 0 → Ring 3

**Method**: `iretq` instruction

**Stack Layout for iretq**:
```
Higher addresses
+0x20: SS      (Stack Segment, Ring 3 data selector)
+0x18: RSP     (User stack pointer)
+0x10: RFLAGS  (Flags register, with IF=1 for interrupts)
+0x08: CS      (Code Segment, Ring 3 code selector)
+0x00: RIP     (Instruction pointer, entry point)
Lower addresses ← RSP points here
```

**Why iretq?**:
- Normal `ret` doesn't change privilege level
- `iretq` is designed for interrupt returns
- Automatically switches CPL based on CS selector
- Restores user stack (RSP, SS)

### 3. Kernel Stack Management

**Problem**: When in Ring 3, CPU needs to know where kernel stack is for interrupts/syscalls.

**Solution**: Task State Segment (TSS)

**TSS Structure** (simplified):
```c
struct TSS {
    uint32_t reserved1;
    uint64_t rsp0;  // Ring 0 stack pointer (IMPORTANT!)
    uint64_t rsp1;  // Ring 1 stack pointer (unused)
    uint64_t rsp2;  // Ring 2 stack pointer (unused)
    // ... more fields
};
```

**How it works**:
1. Process in Ring 3 executes
2. Timer interrupt occurs
3. CPU reads RSP0 from TSS
4. CPU switches to RSP0 (kernel stack)
5. CPU pushes interrupt frame onto kernel stack
6. Timer interrupt handler executes in Ring 0

---

## Implementation Plan

### Phase 3.1: GDT Setup with Ring 3 Segments

**File**: `src/kernel/arch/x86_64/gdt.cpp` (NEW)

**Tasks**:
1. Define GDT entry structure
2. Create 5-entry GDT:
   - Entry 0: Null descriptor (required by x86_64)
   - Entry 1: Kernel code (Ring 0, executable, present)
   - Entry 2: Kernel data (Ring 0, writable, present)
   - Entry 3: User code (Ring 3, executable, present)
   - Entry 4: User data (Ring 3, writable, present)
3. Define GDT pointer structure
4. Implement `initialize_gdt()` function
5. Load GDT with `lgdt` instruction

**Code Structure**:
```cpp
namespace xinim::kernel {

struct GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct GdtPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// 5 entries: null, kernel code, kernel data, user code, user data
static GdtEntry g_gdt[5];
static GdtPointer g_gdt_ptr;

void initialize_gdt();

} // namespace xinim::kernel
```

**Selectors**:
- Kernel Code: 0x08 (entry 1, RPL=0)
- Kernel Data: 0x10 (entry 2, RPL=0)
- User Code: 0x1B (entry 3, RPL=3)
- User Data: 0x23 (entry 4, RPL=3)

---

### Phase 3.2: TSS Setup for Kernel Stack Switching

**File**: `src/kernel/arch/x86_64/tss.cpp` (NEW)

**Tasks**:
1. Define TSS structure (104 bytes on x86_64)
2. Create global TSS
3. Add TSS descriptor to GDT (entry 5, spans 2 entries in 64-bit mode)
4. Implement `set_kernel_stack(uint64_t rsp0)` to update TSS.rsp0
5. Load TSS with `ltr` instruction

**Code Structure**:
```cpp
namespace xinim::kernel {

struct TaskStateSegment {
    uint32_t reserved1;
    uint64_t rsp0;  // Kernel stack pointer (critical!)
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist[7];  // Interrupt Stack Table
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed));

static TaskStateSegment g_tss;

void initialize_tss();
void set_kernel_stack(uint64_t rsp0);

} // namespace xinim::kernel
```

**Why TSS?**:
- When interrupt occurs in Ring 3, CPU loads RSP from TSS.rsp0
- This gives us a valid kernel stack for interrupt handling
- Without this, interrupts in Ring 3 would crash (invalid stack)

---

### Phase 3.3: Per-Process Kernel Stacks

**File**: `src/kernel/server_spawn.cpp` (MODIFY)

**Problem**: Each process needs its own kernel stack for interrupt handling.

**Solution**:
1. Allocate kernel stack during process creation (4 KB per process)
2. Store kernel stack pointer in PCB
3. Update TSS.rsp0 before context switching to new process

**PCB Changes**:
```cpp
// Add to ProcessControlBlock in pcb.hpp
struct ProcessControlBlock {
    // ... existing fields ...

    void* kernel_stack_base;    // NEW: Base of kernel stack
    uint64_t kernel_stack_size; // NEW: Kernel stack size (4 KB)
    uint64_t kernel_rsp;        // NEW: Current kernel stack pointer

    // ... rest of fields ...
};
```

**Workflow**:
```
1. spawn_server():
   - Allocate user stack (16 KB)
   - Allocate kernel stack (4 KB)
   - Set kernel_rsp = kernel_stack_base + 4096

2. scheduler switch_to():
   - Before context_switch(), call set_kernel_stack(next->kernel_rsp)
   - CPU will use this stack when interrupt occurs
```

---

### Phase 3.4: Ring 3 Context Initialization

**File**: `src/kernel/server_spawn.cpp` (MODIFY)

**Change**: Use Ring 3 selectors in context initialization

**Before** (Ring 0):
```cpp
pcb->context.initialize(entry_point, stack_ptr, 0);  // Ring 0
```

**After** (Ring 3):
```cpp
pcb->context.initialize(entry_point, stack_ptr, 3);  // Ring 3
```

**What this does**:
- Sets CS = 0x1B (user code segment, Ring 3)
- Sets SS = 0x23 (user data segment, Ring 3)
- Sets DS, ES, FS, GS = 0x23 (all user data)
- Sets RFLAGS with IF=1 (interrupts enabled)

---

### Phase 3.5: Ring 3 Context Loading

**File**: `src/kernel/scheduler.cpp` (MODIFY)

**Change**: Use `load_context_ring3()` for first-time Ring 3 process launch

**Current Code**:
```cpp
void start_scheduler() {
    // ...
    load_context(&first->context);  // Ring 0 entry
}
```

**New Code**:
```cpp
void start_scheduler() {
    // ...
    // Check if first process should run in Ring 3
    if (first->context.cs == 0x1B) {  // User code segment
        load_context_ring3(&first->context);  // Ring 3 entry
    } else {
        load_context(&first->context);  // Ring 0 entry
    }
}
```

**What this does**:
- `load_context_ring3()` sets up interrupt frame for iretq
- CPU performs privilege level switch (Ring 0 → Ring 3)
- Process begins execution in user mode

---

### Phase 3.6: Timer Interrupt Kernel Stack Switching

**File**: `src/kernel/scheduler.cpp` (MODIFY)

**Add**: Update TSS.rsp0 during context switch

**Code**:
```cpp
static void switch_to(ProcessControlBlock* next) {
    ProcessControlBlock* current = g_current_process;

    if (current == next) return;

    // ... state management ...

    // NEW: Update kernel stack for next process
    set_kernel_stack(next->kernel_rsp);

    // Perform actual context switch
    if (current) {
        context_switch(&current->context, &next->context);
    } else {
        if (next->context.cs == 0x1B) {
            load_context_ring3(&next->context);
        } else {
            load_context(&next->context);
        }
    }
}
```

**Why**:
- When timer interrupt occurs, CPU needs valid kernel stack
- TSS.rsp0 tells CPU where to find it
- Must be updated on every context switch

---

## Testing Strategy

### Test 1: GDT and TSS Initialization
```cpp
void test_gdt_tss() {
    initialize_gdt();
    initialize_tss();

    // Verify GDT loaded
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    assert(cs == 0x08);  // Kernel code segment

    // Verify TSS loaded
    uint16_t tr;
    __asm__ volatile ("str %0" : "=r"(tr));
    assert(tr == 0x28);  // TSS descriptor
}
```

### Test 2: Ring 3 Execution
```cpp
// Simple Ring 3 test function
extern "C" void ring3_test_function() {
    // Try to read CS register
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));

    // Should be 0x1B (Ring 3)
    if ((cs & 3) == 3) {
        early_serial.write("[TEST] Running in Ring 3!\n");
    } else {
        early_serial.write("[ERROR] Not in Ring 3!\n");
    }

    // Loop forever
    for(;;) { __asm__ volatile ("hlt"); }
}

void test_ring3_execution() {
    // Spawn test process in Ring 3
    ProcessControlBlock* pcb = create_test_pcb();
    pcb->context.initialize((uint64_t)ring3_test_function,
                           (uint64_t)stack_top,
                           3);  // Ring 3

    load_context_ring3(&pcb->context);
}
```

### Test 3: Ring 3 → Ring 0 Interrupt
```cpp
void test_ring3_interrupt() {
    // Start Ring 3 process
    // Wait for timer interrupt
    // Verify interrupt handler runs in Ring 0
    // Verify return to Ring 3

    // Check CPL (Current Privilege Level)
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    assert((cs & 3) == 0);  // Interrupt handler in Ring 0
}
```

---

## File Changes Summary

### New Files (2)
1. `src/kernel/arch/x86_64/gdt.cpp` (~200 LOC)
   - GDT structure and initialization
   - 5 GDT entries (null, kernel code/data, user code/data)

2. `src/kernel/arch/x86_64/tss.cpp` (~150 LOC)
   - TSS structure and initialization
   - set_kernel_stack() function

### Modified Files (4)
1. `src/kernel/pcb.hpp` (+3 fields)
   - Add kernel_stack_base, kernel_stack_size, kernel_rsp

2. `src/kernel/server_spawn.cpp` (~+50, -10)
   - Allocate kernel stacks
   - Initialize contexts for Ring 3
   - Set kernel_rsp in PCB

3. `src/kernel/scheduler.cpp` (~+15, -5)
   - Call set_kernel_stack() during switch_to()
   - Use load_context_ring3() for Ring 3 processes

4. `src/kernel/main.cpp` (~+5, 0)
   - Call initialize_gdt() and initialize_tss()

### Total Estimated LOC
- **New**: ~350 LOC
- **Modified**: ~+70, -15 = +55 net
- **Total**: ~405 LOC

---

## Potential Issues and Mitigations

### Issue 1: Invalid Kernel Stack
**Problem**: If TSS.rsp0 is wrong, interrupt in Ring 3 will triple fault
**Mitigation**:
- Always allocate kernel stack during process creation
- Verify kernel_rsp is valid (within allocated range)
- Add assertions in set_kernel_stack()

### Issue 2: Segment Selector Typos
**Problem**: Using 0x18 instead of 0x1B will cause general protection fault
**Mitigation**:
- Define constants: USER_CS = 0x1B, USER_DS = 0x23
- Use constants instead of magic numbers
- Add comments explaining selector format

### Issue 3: RFLAGS Not Set Correctly
**Problem**: If IF (interrupt flag) not set, interrupts won't work in Ring 3
**Mitigation**:
- context.initialize() should set RFLAGS = 0x202 (IF=1, reserved bit=1)
- Verify RFLAGS in tests

### Issue 4: User Stack Not Aligned
**Problem**: Misaligned stack can cause crashes
**Mitigation**:
- Align all stacks to 16 bytes (x86_64 ABI requirement)
- Round stack size up to 16-byte boundary

---

## Expected Boot Sequence (Phase 3)

```
1. Kernel _start() entry
2. Initialize GDT with Ring 3 segments
3. Initialize TSS with kernel stack
4. Initialize scheduler
5. Spawn VFS Server:
   - Allocate user stack (16 KB)
   - Allocate kernel stack (4 KB)
   - Initialize context for Ring 3 (CS=0x1B, SS=0x23)
   - Set kernel_rsp in PCB
6. Spawn Process Manager (same as VFS)
7. Spawn Memory Manager (same as VFS)
8. Start scheduler:
   - Pick first process (VFS)
   - Set TSS.rsp0 = vfs_pcb->kernel_rsp
   - Call load_context_ring3(&vfs_pcb->context)
   - CPU performs Ring 0 → Ring 3 transition
   - VFS server begins execution in USER MODE
9. Timer interrupt occurs:
   - CPU reads TSS.rsp0 (VFS kernel stack)
   - CPU switches to kernel stack
   - CPU pushes interrupt frame
   - timer_interrupt_handler executes in Ring 0
   - Scheduler picks next process
   - Set TSS.rsp0 for next process
   - Context switch to next process
   - Return to Ring 3
10. System running with full Ring 3 isolation!
```

---

## Success Criteria

Phase 3 is **COMPLETE** when:

- [ ] GDT created with 5 entries (null, kernel code/data, user code/data)
- [ ] TSS created and loaded with `ltr`
- [ ] Per-process kernel stacks allocated
- [ ] Servers spawn with Ring 3 context (CS=0x1B, SS=0x23)
- [ ] load_context_ring3() successfully transitions to Ring 3
- [ ] Timer interrupts work in Ring 3 (use kernel stack from TSS)
- [ ] Context switching updates TSS.rsp0 correctly
- [ ] Test shows CS & 3 == 3 (running in Ring 3)
- [ ] All code compiles without errors
- [ ] Documentation complete

---

## Timeline

**Total**: 4-6 hours

- **Hour 1**: GDT implementation and testing
- **Hour 2**: TSS implementation and testing
- **Hour 3**: Per-process kernel stacks
- **Hour 4**: Ring 3 context initialization
- **Hour 5**: Integration and testing
- **Hour 6**: Documentation and commit

---

## Next: Phase 4 (Syscalls)

Once Ring 3 is working, Phase 4 will add:
1. Syscall/sysret handlers
2. Syscall dispatch table
3. Integration with Lattice IPC
4. User → Kernel transitions via syscall

---

**Status**: 🚧 Planning Complete, Ready to Execute!

**Next**: Begin implementation with GDT setup 🚀

# Week 8: Preemptive Scheduling & Ring 3 Transition - Complete Plan

**Date**: November 17, 2025
**Status**: 🚀 IN PROGRESS
**Branch**: `claude/audit-translate-repo-018NNYQuW6XJrQJL2LLUPS45`
**Estimated Effort**: 24-30 hours

---

## Executive Summary

Week 8 transforms XINIM from a cooperative, Ring 0 system into a **fully preemptive microkernel** with proper privilege separation. This is the most complex week yet, involving:

1. **Preemptive Scheduling**: Timer interrupts trigger context switches
2. **Ring 3 Transition**: Servers move from kernel mode to user mode
3. **Syscall Infrastructure**: Proper entry/exit mechanisms (syscall/sysret)
4. **Complete IPC Integration**: Message passing with scheduling integration
5. **ELF Loader**: Load and execute init process (PID 1)

By the end of Week 8, XINIM will be a **production-ready microkernel** capable of running real userspace programs with full memory isolation and preemptive multitasking.

---

## Strategic Phasing

Week 8 is divided into **5 phases**, executed in dependency order:

```
Phase 1: Context Switching (Foundation)
  ↓
Phase 2: Timer Interrupts (Preemption)
  ↓
Phase 3: Ring 3 Transition (Privilege Separation)
  ↓
Phase 4: Syscall Infrastructure (User→Kernel transitions)
  ↓
Phase 5: IPC Integration & Testing (Full System)
```

**Critical Path**: Context switching must work before preemption; Ring 3 must work before syscalls.

---

## Phase 1: Context Switching Infrastructure (6-8 hours)

### 1.1 Full CPU Context Structure

**File**: `src/kernel/context.hpp` (NEW - 150 LOC)

**Purpose**: Define complete CPU state for x86_64/ARM64.

```cpp
/**
 * @brief Complete CPU context for x86_64
 *
 * Stores ALL registers needed for full context switch.
 * This is saved/restored on every timer interrupt.
 */
struct CpuContext_x86_64 {
    // General Purpose Registers
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    // Instruction pointer and flags
    uint64_t rip;
    uint64_t rflags;

    // Segment selectors
    uint64_t cs, ss, ds, es, fs, gs;

    // Floating point state (if FPU is used)
    // uint8_t fxsave_area[512] __attribute__((aligned(16)));

    // Control registers (for kernel threads)
    uint64_t cr3;  // Page directory base (for user processes)
};

#ifdef XINIM_ARCH_X86_64
using CpuContext = CpuContext_x86_64;
#elif defined(XINIM_ARCH_ARM64)
using CpuContext = CpuContext_arm64;
#endif
```

**Size**: 168 bytes (without FPU state), 680 bytes (with FPU)

**Week 8 Scope**: Skip FPU state for simplicity. Week 9 will add lazy FPU switching.

---

### 1.2 Context Switch Assembly

**File**: `src/arch/x86_64/context_switch.S` (NEW - 200 LOC)

**Purpose**: Low-level assembly for saving/restoring CPU state.

```asm
/**
 * @brief Switch from current process to next process
 * @param rdi Pointer to current process's CpuContext*
 * @param rsi Pointer to next process's CpuContext*
 *
 * C signature: void context_switch(CpuContext* current, CpuContext* next);
 */
.global context_switch
.type context_switch, @function
context_switch:
    // Save current context
    mov [rdi + 0x00], rax
    mov [rdi + 0x08], rbx
    mov [rdi + 0x10], rcx
    mov [rdi + 0x18], rdx
    mov [rdi + 0x20], rsi
    mov [rdi + 0x28], rdi
    mov [rdi + 0x30], rbp
    mov [rdi + 0x38], rsp
    mov [rdi + 0x40], r8
    mov [rdi + 0x48], r9
    mov [rdi + 0x50], r10
    mov [rdi + 0x58], r11
    mov [rdi + 0x60], r12
    mov [rdi + 0x68], r13
    mov [rdi + 0x70], r14
    mov [rdi + 0x78], r15

    // Save RIP (return address is on stack)
    mov rax, [rsp]
    mov [rdi + 0x80], rax

    // Save RFLAGS
    pushfq
    pop rax
    mov [rdi + 0x88], rax

    // Save segment selectors
    mov ax, cs
    mov [rdi + 0x90], rax
    mov ax, ss
    mov [rdi + 0x98], rax
    // ds, es, fs, gs...

    // Save CR3 (page table base)
    mov rax, cr3
    mov [rdi + 0xA8], rax

    // Restore next context
    mov rax, [rsi + 0xA8]  // CR3
    mov cr3, rax            // Switch page tables

    // Restore general purpose registers
    mov rax, [rsi + 0x00]
    mov rbx, [rsi + 0x08]
    mov rcx, [rsi + 0x10]
    // ... all registers

    // Restore stack pointer
    mov rsp, [rsi + 0x38]

    // Restore RFLAGS
    mov rax, [rsi + 0x88]
    push rax
    popfq

    // Jump to next RIP
    jmp [rsi + 0x80]
```

**Critical Details**:
- Saves **ALL** registers (no caller-save assumptions)
- Switches page tables (CR3) for memory isolation
- Restores stack pointer before jumping
- Uses `jmp` instead of `ret` (RIP is absolute address)

---

### 1.3 Scheduler Context Switch Integration

**File**: `src/kernel/scheduler.cpp` (MODIFIED - add ~100 LOC)

**Current** (Week 7):
```cpp
void schedule_forever() {
    ProcessControlBlock* current = g_ready_queue_head;
    while (current) {
        auto entry_fn = reinterpret_cast<void(*)()>(current->context.rip);
        entry_fn();  // Call directly (never returns)
        current = current->next;
    }
}
```

**New** (Week 8):
```cpp
extern "C" void context_switch(CpuContext* current, CpuContext* next);

static ProcessControlBlock* g_current_process = nullptr;
static ProcessControlBlock* g_idle_process = nullptr;

/**
 * @brief Pick next process to run (round-robin for Week 8)
 */
static ProcessControlBlock* pick_next_process() {
    if (!g_current_process) {
        return g_ready_queue_head;
    }

    // Round-robin: next in list, or wrap to head
    ProcessControlBlock* next = g_current_process->next;
    if (!next) {
        next = g_ready_queue_head;
    }

    return next ? next : g_idle_process;
}

/**
 * @brief Perform context switch (called from timer interrupt)
 */
void schedule() {
    ProcessControlBlock* current = g_current_process;
    ProcessControlBlock* next = pick_next_process();

    if (current == next) {
        return;  // Nothing to do
    }

    // Save current state, switch to next
    g_current_process = next;
    next->state = ProcessState::RUNNING;

    if (current) {
        current->state = ProcessState::READY;
        context_switch(&current->context, &next->context);
    } else {
        // First time - just load next context
        // (Cannot save current because we don't have a valid context yet)
        load_context(&next->context);
    }
}
```

**Key Changes**:
- Round-robin scheduling (simple, fair)
- Tracks `g_current_process`
- State transitions: RUNNING ↔ READY
- Calls assembly `context_switch()`

---

## Phase 2: Timer Interrupt Handler (4-6 hours)

### 2.1 Interrupt Handler Assembly

**File**: `src/arch/x86_64/interrupts.S` (MODIFIED - add ~150 LOC)

**Purpose**: Save context, call scheduler, restore context.

```asm
/**
 * @brief Timer interrupt handler (IRQ 0, vector 32)
 *
 * This is called by hardware ~100 times per second.
 * It saves the current process context, calls the scheduler,
 * and restores the next process context.
 */
.global timer_interrupt_handler
.type timer_interrupt_handler, @function
.align 16
timer_interrupt_handler:
    // Hardware pushes: SS, RSP, RFLAGS, CS, RIP
    // We need to save the rest

    // Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    // Save segment selectors
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax

    // Load kernel data segment
    mov ax, 0x10  // Kernel data segment
    mov ds, ax
    mov es, ax

    // RSP now points to complete context on stack
    // Call C++ scheduler
    mov rdi, rsp  // Pass context pointer as argument
    call timer_interrupt_c_handler

    // Restore segment selectors
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    // Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    // Send EOI to APIC
    mov rdi, 0xFEE000B0  // APIC EOI register (virtual address)
    mov dword ptr [rdi], 0

    // Return from interrupt (pops SS, RSP, RFLAGS, CS, RIP)
    iretq
```

**Critical Details**:
- Saves context **on stack** (easier than using PCB directly)
- Switches to kernel data segment
- Calls C++ handler
- Sends EOI (End of Interrupt) to APIC
- Uses `iretq` to return (restores RFLAGS, CS, RIP, SS, RSP)

---

### 2.2 C++ Timer Handler

**File**: `src/kernel/timer.cpp` (NEW - 100 LOC)

```cpp
/**
 * @brief C++ timer interrupt handler
 *
 * Called from assembly with pointer to saved context on stack.
 */
extern "C" void timer_interrupt_c_handler(CpuContext* context) {
    // Increment global tick counter
    g_ticks++;

    // Get current process
    ProcessControlBlock* current = g_current_process;

    if (current && current->state == ProcessState::RUNNING) {
        // Save current context from stack into PCB
        memcpy(&current->context, context, sizeof(CpuContext));
    }

    // Call scheduler to pick next process
    schedule();

    // Scheduler has switched contexts - when we return,
    // the assembly stub will restore the NEW process's context
}
```

**Key Points**:
- Copies stack context into PCB
- Calls `schedule()` to pick next process
- `schedule()` performs context switch
- When we return, assembly restores **next** process's context

---

### 2.3 Timer Initialization

**File**: `src/kernel/timer.cpp` (MODIFIED)

**Setup APIC timer** (already partially done in main.cpp):

```cpp
void initialize_timer_interrupts() {
    // Set up IDT entry for vector 32 (timer)
    idt_set_entry(32, (uint64_t)timer_interrupt_handler,
                  0x08,  // Kernel code segment
                  0x8E); // Present, DPL=0, Interrupt Gate

    // APIC timer already configured in main.cpp
    // Just ensure it's sending interrupts to vector 32

    early_serial.write("[TIMER] Preemptive scheduling enabled (100 Hz)\n");
}
```

---

## Phase 3: Ring 3 Transition (5-7 hours)

### 3.1 GDT Setup for Ring 3

**File**: `src/arch/x86_64/gdt.cpp` (MODIFIED - add ~50 LOC)

**Current GDT** (Week 7):
```
0: Null descriptor
1: Kernel code (0x08, Ring 0)
2: Kernel data (0x10, Ring 0)
```

**New GDT** (Week 8):
```
0: Null descriptor
1: Kernel code (0x08, Ring 0, executable)
2: Kernel data (0x10, Ring 0, writable)
3: User code   (0x18, Ring 3, executable)  ← NEW
4: User data   (0x20, Ring 3, writable)    ← NEW
5: TSS (Task State Segment)                ← NEW (for stack switching)
```

**Code**:
```cpp
struct GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

static GdtEntry gdt[6];

void setup_gdt() {
    // ... existing kernel entries ...

    // User code segment (Ring 3)
    gdt[3].limit_low = 0xFFFF;
    gdt[3].base_low = 0;
    gdt[3].base_mid = 0;
    gdt[3].base_high = 0;
    gdt[3].access = 0xFA;      // Present, DPL=3, Executable, Readable
    gdt[3].granularity = 0xCF; // 4KB granularity, 64-bit

    // User data segment (Ring 3)
    gdt[4].limit_low = 0xFFFF;
    gdt[4].base_low = 0;
    gdt[4].base_mid = 0;
    gdt[4].base_high = 0;
    gdt[4].access = 0xF2;      // Present, DPL=3, Writable
    gdt[4].granularity = 0xCF;

    // TSS (for stack switching)
    setup_tss(&gdt[5]);

    // Load new GDT
    load_gdt(gdt, sizeof(gdt));
}
```

**Segment Selectors**:
- `0x08` = Kernel code (Ring 0)
- `0x10` = Kernel data (Ring 0)
- `0x18 | 3 = 0x1B` = User code (Ring 3) - note the `| 3` for RPL
- `0x20 | 3 = 0x23` = User data (Ring 3)

---

### 3.2 Task State Segment (TSS)

**Purpose**: TSS stores kernel stack pointer for Ring 3 → Ring 0 transitions.

**File**: `src/arch/x86_64/tss.cpp` (NEW - 100 LOC)

```cpp
struct TSS {
    uint32_t reserved0;
    uint64_t rsp0;  // Kernel stack pointer (for Ring 3 → Ring 0)
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];  // Interrupt stack table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static TSS tss;
static uint8_t kernel_stack[8192] __attribute__((aligned(16)));

void setup_tss(GdtEntry* tss_entry) {
    memset(&tss, 0, sizeof(TSS));

    // Set kernel stack pointer
    tss.rsp0 = (uint64_t)&kernel_stack[8192];

    // Set up GDT entry for TSS
    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof(TSS) - 1;

    tss_entry->limit_low = limit & 0xFFFF;
    tss_entry->base_low = base & 0xFFFF;
    tss_entry->base_mid = (base >> 16) & 0xFF;
    tss_entry->access = 0x89;      // Present, DPL=0, TSS
    tss_entry->granularity = 0x00; // Byte granularity
    tss_entry->base_high = (base >> 24) & 0xFF;

    // Load TSS
    __asm__ volatile ("ltr %%ax" :: "a"(0x28));  // Selector 5 (0x28)
}
```

**Why TSS?**
- When syscall happens (Ring 3 → Ring 0), CPU loads `rsp0` from TSS
- This ensures kernel has a valid stack
- Without TSS, kernel would use user stack (security hole!)

---

### 3.3 Transition Server to Ring 3

**File**: `src/kernel/server_spawn.cpp` (MODIFIED)

**Week 7** (Ring 0):
```cpp
pcb->context.cs = 0x08;  // Kernel code
pcb->context.ss = 0x10;  // Kernel data
```

**Week 8** (Ring 3):
```cpp
pcb->context.cs = 0x1B;  // User code (0x18 | 3)
pcb->context.ss = 0x23;  // User data (0x20 | 3)
pcb->context.rflags = 0x202;  // IF=1, IOPL=0
```

**First Context Load**:
```cpp
/**
 * @brief Load context and jump to Ring 3
 *
 * This is used for the FIRST time a process runs.
 * Cannot use context_switch because there's no previous context.
 */
void load_context_ring3(CpuContext* ctx) {
    __asm__ volatile (
        "mov %0, %%rsp\n"      // Load context pointer
        "pop %%rax\n"          // Restore registers
        "pop %%rbx\n"
        // ... all registers ...
        "iretq"                // Return to Ring 3
        :: "r"(ctx)
    );
}
```

---

## Phase 4: Syscall Infrastructure (6-8 hours)

### 4.1 SYSCALL/SYSRET Setup

**File**: `src/arch/x86_64/syscall_init.cpp` (NEW - 150 LOC)

**Purpose**: Configure CPU for fast syscalls.

```cpp
/**
 * @brief Initialize syscall/sysret mechanism
 *
 * Sets up MSRs (Model Specific Registers) for fast syscalls.
 */
void initialize_syscall() {
    // IA32_STAR: Syscall target selectors
    // Bits 32-47: Kernel CS/SS
    // Bits 48-63: User CS/SS
    uint64_t star = ((uint64_t)0x08 << 32) |  // Kernel CS
                    ((uint64_t)0x18 << 48);    // User CS (will be +8 for SS)
    wrmsr(0xC0000081, star);

    // IA32_LSTAR: Syscall target RIP
    wrmsr(0xC0000082, (uint64_t)syscall_entry);

    // IA32_FMASK: RFLAGS mask (clear IF during syscall)
    wrmsr(0xC0000084, 0x200);  // Clear IF

    // Enable SYSCALL in EFER (Extended Feature Enable Register)
    uint64_t efer = rdmsr(0xC0000080);
    efer |= (1 << 0);  // SCE bit
    wrmsr(0xC0000080, efer);

    early_serial.write("[SYSCALL] Fast syscalls enabled (syscall/sysret)\n");
}
```

**Key MSRs**:
- `IA32_STAR` (0xC0000081): Segment selectors
- `IA32_LSTAR` (0xC0000082): Syscall entry point
- `IA32_FMASK` (0xC0000084): RFLAGS mask

---

### 4.2 Syscall Entry Point

**File**: `src/arch/x86_64/syscall_entry.S` (NEW - 300 LOC)

**Purpose**: Save user context, call dispatcher, restore context, return to user.

```asm
/**
 * @brief Syscall entry point
 *
 * When userspace executes 'syscall', CPU jumps here.
 * RCX = user RIP (return address)
 * R11 = user RFLAGS
 * RAX = syscall number
 * RDI, RSI, RDX, R10, R8, R9 = arguments
 */
.global syscall_entry
.align 16
syscall_entry:
    // Switch to kernel stack (from TSS.rsp0)
    swapgs              // Swap GS base (user GS ↔ kernel GS)
    mov [gs:0], rsp     // Save user RSP in per-CPU area
    mov rsp, [gs:8]     // Load kernel RSP from per-CPU area

    // Save user context on kernel stack
    push 0x23           // User SS
    push qword ptr [gs:0]  // User RSP
    push r11            // User RFLAGS (saved by CPU in R11)
    push 0x1B           // User CS
    push rcx            // User RIP (saved by CPU in RCX)

    // Save syscall arguments
    push rax            // Syscall number
    push rdi            // Arg 0
    push rsi            // Arg 1
    push rdx            // Arg 2
    push r10            // Arg 3
    push r8             // Arg 4
    push r9             // Arg 5

    // Call C++ syscall dispatcher
    mov rdi, rax        // Syscall number
    mov rsi, rdi        // Arg 0 (but rdi was saved, need to load from stack)
    // Actually, pass stack pointer and let C++ function extract args
    mov rdi, rsp
    call syscall_dispatcher_c

    // RAX now contains return value

    // Restore user context
    add rsp, 6*8        // Skip saved arguments (they're not used for return)

    // Restore user registers from stack
    pop rcx             // User RIP
    add rsp, 8          // Skip user CS
    pop r11             // User RFLAGS
    pop rsp             // User RSP (this is tricky - RSP is changing!)

    // Actually, we need to save RSP elsewhere first
    // Let me redo this properly:

syscall_entry_proper:
    swapgs
    mov [gs:0], rsp     // Save user RSP
    mov rsp, [gs:8]     // Load kernel stack

    // Build interrupt frame on kernel stack
    push 0x23           // User SS
    push qword ptr [gs:0]  // User RSP
    push r11            // User RFLAGS
    push 0x1B           // User CS
    push rcx            // User RIP

    // Save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    // Call dispatcher
    mov rdi, rsp  // Pass pointer to saved context
    call syscall_dispatcher_c

    // Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    // Don't pop RAX (it has return value)
    add rsp, 8

    // Return to user mode
    swapgs
    sysretq  // Returns to RCX with RFLAGS from R11
```

**SYSCALL mechanism**:
1. User calls `syscall` instruction
2. CPU saves RIP → RCX, RFLAGS → R11
3. CPU loads kernel RIP from LSTAR, kernel CS from STAR
4. We switch to kernel stack (via GS segment)
5. Save all registers
6. Call C++ dispatcher
7. Restore registers (RAX has return value)
8. `sysretq` returns to user (RIP from RCX, RFLAGS from R11)

---

### 4.3 C++ Syscall Dispatcher

**File**: `src/kernel/sys/dispatch.cpp` (MODIFIED)

**Add wrapper for syscall entry**:

```cpp
/**
 * @brief C++ syscall dispatcher (called from assembly)
 * @param context Pointer to saved register context on stack
 * @return Syscall return value (stored in RAX)
 */
extern "C" uint64_t syscall_dispatcher_c(SavedContext* context) {
    uint64_t syscall_num = context->rax;
    uint64_t arg0 = context->rdi;
    uint64_t arg1 = context->rsi;
    uint64_t arg2 = context->rdx;
    uint64_t arg3 = context->r10;  // Note: R10, not RCX (RCX has RIP)
    uint64_t arg4 = context->r8;

    // Call existing dispatcher
    uint64_t result = xinim_syscall_dispatch(
        syscall_num, arg0, arg1, arg2, arg3, arg4
    );

    // Store result in RAX (will be restored by assembly)
    context->rax = result;

    return result;
}
```

---

## Phase 5: IPC Integration & Testing (5-7 hours)

### 5.1 IPC with Scheduler Integration

**Challenge**: When a process calls `lattice_recv()`, it should **block** until a message arrives.

**Solution**: Add BLOCKED state to scheduler.

**File**: `src/kernel/lattice_ipc.cpp` (MODIFIED)

```cpp
int lattice_recv(pid_t pid, message* msg, IpcFlags flags) {
    // Check message queue for this PID
    if (!has_pending_message(pid)) {
        // No message available - block the process
        ProcessControlBlock* current = get_current_process();
        current->state = ProcessState::BLOCKED;
        current->blocked_on = BlockReason::IPC_RECV;
        current->ipc_wait_source = ANY_SOURCE;  // Or specific source

        // Yield to scheduler
        schedule();

        // When we wake up, message will be available
    }

    // Dequeue message
    return dequeue_message(pid, msg);
}

int lattice_send(pid_t src, pid_t dst, const message& msg, IpcFlags flags) {
    // Enqueue message for destination
    enqueue_message(dst, msg);

    // Wake up destination if it's blocked on IPC
    ProcessControlBlock* dest_pcb = find_pcb_by_pid(dst);
    if (dest_pcb && dest_pcb->state == ProcessState::BLOCKED &&
        dest_pcb->blocked_on == BlockReason::IPC_RECV) {
        dest_pcb->state = ProcessState::READY;
        // Scheduler will pick it up on next tick
    }

    return 0;
}
```

**Key Changes**:
- Processes block when no message available
- Sending a message wakes up blocked receivers
- Scheduler only picks READY or RUNNING processes

---

### 5.2 Complete Boot Sequence (Week 8)

```
1. Kernel initialization
   - Hardware, GDT, IDT, memory

2. Setup Ring 3
   - GDT entries for user code/data
   - TSS with kernel stack
   - SYSCALL/SYSRET configuration

3. Initialize timer
   - APIC timer at 100 Hz
   - Timer interrupt handler (vector 32)

4. Spawn servers (Ring 3)
   - VFS (PID 2): CS=0x1B, SS=0x23
   - Process Manager (PID 3)
   - Memory Manager (PID 4)

5. Enable preemption
   - Start timer interrupts
   - All three servers run concurrently

6. Servers enter IPC loops
   - lattice_recv() blocks until message
   - Timer preempts servers

7. (Optional) Load init process
   - ELF loader reads /sbin/init
   - Create process with PID 1
   - Jump to entry point
```

---

## Phase 6: ELF Loader (Optional for Week 8)

**File**: `src/kernel/elf_loader.cpp` (NEW - 400 LOC)

**Purpose**: Load ELF binaries from VFS into memory.

```cpp
/**
 * @brief Load ELF binary and create process
 * @param path Path to ELF file (e.g., "/sbin/init")
 * @param argv Command-line arguments
 * @param envp Environment variables
 * @return PID of new process, or -1 on error
 */
pid_t load_elf(const char* path, char* const argv[], char* const envp[]) {
    // 1. Open file via VFS
    int fd = sys_open_impl(path, O_RDONLY, 0);
    if (fd < 0) return -1;

    // 2. Read ELF header
    Elf64_Ehdr ehdr;
    sys_read_impl(fd, &ehdr, sizeof(ehdr));

    // 3. Validate ELF magic
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        return -1;  // Not an ELF file
    }

    // 4. Read program headers
    Elf64_Phdr phdr[ehdr.e_phnum];
    sys_lseek_impl(fd, ehdr.e_phoff, SEEK_SET);
    sys_read_impl(fd, phdr, sizeof(phdr));

    // 5. Allocate page table for new process
    // (Week 8: Use kernel page table, Week 9: Per-process)

    // 6. Load segments into memory
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // Map virtual address
            void* vaddr = (void*)phdr[i].p_vaddr;
            uint64_t size = phdr[i].p_memsz;

            // Allocate memory (via Memory Manager)
            sys_mmap_impl(vaddr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_FIXED | MAP_ANONYMOUS, -1, 0);

            // Read segment from file
            sys_lseek_impl(fd, phdr[i].p_offset, SEEK_SET);
            sys_read_impl(fd, vaddr, phdr[i].p_filesz);

            // Zero BSS section
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((char*)vaddr + phdr[i].p_filesz, 0,
                       phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    // 7. Set up stack
    void* stack = sys_mmap_impl(nullptr, 8192, PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS, -1, 0);

    // 8. Create PCB
    ProcessControlBlock* pcb = create_process();
    pcb->context.rip = ehdr.e_entry;  // Entry point
    pcb->context.rsp = (uint64_t)stack + 8192;
    pcb->context.cs = 0x1B;
    pcb->context.ss = 0x23;

    // 9. Add to scheduler
    scheduler_add_process(pcb);

    sys_close_impl(fd);
    return pcb->pid;
}
```

**Week 8 Scope**: Implement basic ELF loading. Week 9 will add:
- Proper argument/environment setup
- Dynamic linking
- Shared libraries

---

## Testing Strategy (Week 8)

### Test 1: Context Switching

**File**: `tests/context_switch_test.cpp`

```cpp
void test_context_switch() {
    CpuContext ctx1{}, ctx2{};

    // Initialize ctx1 to execute function1
    ctx1.rip = (uint64_t)function1;
    ctx1.rsp = (uint64_t)stack1 + 4096;

    // Initialize ctx2 to execute function2
    ctx2.rip = (uint64_t)function2;
    ctx2.rsp = (uint64_t)stack2 + 4096;

    // Switch from ctx1 to ctx2
    context_switch(&ctx1, &ctx2);

    // Should now be executing function2
    assert(g_current_function == 2);
}
```

### Test 2: Timer Interrupts

**Monitor serial output**:
```
[TIMER] Tick 0
[SCHEDULER] Running VFS (PID 2)
[TIMER] Tick 1
[SCHEDULER] Running Process Manager (PID 3)
[TIMER] Tick 2
[SCHEDULER] Running Memory Manager (PID 4)
[TIMER] Tick 3
[SCHEDULER] Running VFS (PID 2)
```

### Test 3: Ring 3 Transition

**Create test user program**:
```c
// userland/tests/ring3_test.c
#include <stdio.h>

int main() {
    // If we reach here, we're in Ring 3!
    printf("Hello from Ring 3!\n");

    // Test syscall
    getpid();

    return 0;
}
```

### Test 4: Full Syscall Flow

**Test**: Call `open()` from Ring 3, verify VFS server receives message.

```c
int main() {
    int fd = open("/test.txt", O_CREAT | O_RDWR, 0644);
    printf("Opened FD: %d\n", fd);
    return 0;
}
```

**Expected flow**:
1. User calls `open()` (syscall 2)
2. CPU switches to kernel (via `syscall`)
3. Kernel dispatcher calls `sys_open_impl()`
4. `sys_open_impl()` sends IPC to VFS (PID 2)
5. VFS server receives message, creates file
6. VFS sends response with FD
7. Kernel returns FD to user (via `sysret`)
8. User receives FD

---

## Success Criteria (Week 8)

- [ ] Context switching works between kernel threads
- [ ] Timer interrupts fire at 100 Hz
- [ ] Scheduler preempts processes (round-robin)
- [ ] All three servers execute concurrently
- [ ] GDT has Ring 3 entries
- [ ] TSS configured with kernel stack
- [ ] Servers run in Ring 3 (CS=0x1B, SS=0x23)
- [ ] SYSCALL/SYSRET configured
- [ ] User can call syscalls from Ring 3
- [ ] IPC messages block/unblock processes correctly
- [ ] At least one full syscall flow validated (open → VFS)
- [ ] (Optional) ELF loader loads init process
- [ ] Documentation complete

---

## Implementation Timeline

| Phase | Task | Hours | Priority |
|-------|------|-------|----------|
| 1 | Context switching infrastructure | 6-8 | CRITICAL |
| 2 | Timer interrupt handler | 4-6 | CRITICAL |
| 3 | Ring 3 GDT/TSS setup | 5-7 | HIGH |
| 4 | Syscall/sysret infrastructure | 6-8 | HIGH |
| 5 | IPC scheduler integration | 5-7 | HIGH |
| 6 | ELF loader (optional) | 6-8 | MEDIUM |
| Documentation | | 2-3 | HIGH |
| **TOTAL** | | **34-47** | |

**Realistic Week 8 Scope** (24-30 hours):
- Phases 1-3: Context switching, timer, Ring 3
- Phase 4: Syscall infrastructure (partial - basic entry/exit)
- Phase 5: IPC integration (basic blocking)
- Skip ELF loader (move to Week 9)

---

## Known Challenges & Mitigations

### Challenge 1: Context Switch Assembly Complexity

**Issue**: Saving/restoring 20+ registers is error-prone.

**Mitigation**:
- Use macros for repetitive code
- Test with single-step debugger
- Validate with context_switch_test

### Challenge 2: Ring 3 Stack Switching

**Issue**: When syscall happens, need to switch from user stack to kernel stack.

**Mitigation**:
- Use TSS.rsp0 (hardware does this automatically)
- Per-CPU GS segment for quick access
- Test with simple syscall first (getpid)

### Challenge 3: Timer Interrupt Re-entrancy

**Issue**: What if timer fires during another interrupt handler?

**Mitigation**:
- Disable interrupts during critical sections
- Use separate interrupt stacks (IST in TSS)
- Week 8: Keep simple, week 9 will add IST

### Challenge 4: IPC Deadlocks

**Issue**: Process A waits for B, B waits for A → deadlock.

**Mitigation**:
- Add timeout to `lattice_recv()`
- Week 8: No timeout, assume correct protocol
- Week 9: Add deadlock detection

---

## Week 9 Preview

After Week 8, XINIM will have:
- ✅ Preemptive scheduling
- ✅ Ring 3 servers with memory isolation
- ✅ Working syscall infrastructure
- ✅ IPC blocking/unblocking

Week 9 will add:
- 🔄 Per-process page tables (full memory isolation)
- 🔄 ELF loader with dynamic linking
- 🔄 Init process (PID 1) spawning
- 🔄 SMP support (multi-core)
- 🔄 Advanced scheduling (priority, real-time)

---

**Let's execute Week 8!** 🚀

---

**Next Command**: Implement Phase 1 (Context Switching)

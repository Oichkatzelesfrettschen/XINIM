# Xinim Microkernel Boundary Specification

Phase 6 baseline: 2026-02-26.

## Principle

Xinim follows the microkernel architecture: the kernel implements only the
mechanisms needed for safe, efficient inter-process communication and resource
protection. Policy belongs in userspace servers.

## Kernel Responsibilities (src/kernel/)

The kernel is responsible for:

1. **Interrupt handling**: IDT, APIC, ISR dispatch (interrupts.cpp, irq.cpp)
2. **Process scheduling**: CFS scheduler, context switches (scheduler.cpp, schedule.cpp)
3. **Paging and physical memory**: Page tables, physical page allocator (paging.cpp, memory.cpp)
4. **IPC mechanism**: Lattice IPC message passing, capability checking (lattice_ipc.cpp)
5. **Syscall gateway**: Ring 0 entry, argument validation, routing to servers (sys/dispatch.cpp)
6. **Early console**: Serial 16550 for debugging (early/serial_16550.cpp)
7. **Timer and clock**: HPET, APIC timer tick (timer.cpp, clock.cpp)
8. **CPU topology**: GDT, TSS, CPUID (arch/x86_64/)
9. **Boot**: Limine protocol parsing, initial stack, BSS clear (boot/limine/)

The kernel does NOT implement: file I/O, process creation policy, memory
allocation policy, network protocol, or device-specific behavior.

## Server Responsibilities

| Server | PID | Location | Responsibility |
|--------|-----|----------|----------------|
| VFS | 2 | src/servers/vfs_server/ | open/read/write/close/lseek/stat on ramfs |
| PM | 3 | src/servers/process_manager/ | fork/exec/exit/wait/kill/signals |
| MM | 4 | src/servers/memory_manager/ | brk/mmap/munmap, process address spaces |
| RS | 1 | src/servers/reincarnation_server.cpp | server health, crash recovery, respawn |

## Current Ring 0 Violation (Known, Documented)

**Status**: All servers currently run in Ring 0 (kernel mode). Ring 3 (user mode)
transition is not yet implemented.

**Why**: x86_64 Ring 3 requires user-mode page tables, syscall/sysret plumbing, and
stack switching. This is Phase 7+ work.

**Impact**: The microkernel isolation guarantee does not hold in the current state.
A server crash will fault the kernel. This is acceptable for the development phase.

**Migration Plan**: See docs/adr/0009-ring3-migration.md (to be created in Phase 7).

## Syscall Routing (Current, Phase 6)

```
User process
    |
    | SYSCALL instruction
    v
Kernel syscall_handler.S (Ring 0 entry)
    |
    v
xinim_syscall_dispatch() [src/kernel/sys/dispatch.cpp]
    |
    +-- Kernel-handled (no IPC): SYS_debug_write, SYS_getpid, SYS_getppid, SYS_exit
    |
    +-- Via IPC to VFS (PID 2): SYS_open, SYS_read, SYS_write, SYS_close
    |
    +-- Via IPC to PM  (PID 3): SYS_fork, SYS_execve, SYS_wait4, SYS_kill
```

## Boundary Violations to Fix

These items currently violate the boundary:

| File | Violation | Phase |
|------|-----------|-------|
| src/kernel/sys/dispatch.cpp:43 | cur_proc hardcoded to 1 instead of from scheduler | Phase 6 |
| src/kernel/sys/dispatch.cpp:69 | SYS_exit halts CPU instead of notifying PM | Phase 6 |
| src/kernel/vfs_interface.cpp | Kernel-side VFS glue; duplicates server responsibility | Phase 6 (ADR 0006) |
| src/fs/ | Legacy MINIX FS code; not integrated; may be superseded | Phase 6 (ADR 0006) |

## Memory Model

```
Physical memory layout (after boot):
0x000000 - 0x0FFFFF: Reserved (BIOS, legacy)
0x100000 - ...:      Kernel image (xinim ELF, 1MB load)
Heap end - ...:      Kernel heap (bump allocator, no-op free)
...      - top:      Available physical frames (page allocator)
```

Servers currently share the kernel address space (Ring 0). Future Ring 3
servers will each have their own page tables with a kernel mapping in the
upper half.

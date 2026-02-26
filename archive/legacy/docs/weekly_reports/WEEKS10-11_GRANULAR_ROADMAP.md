# Weeks 10-11 Granular Implementation Roadmap

**Status**: Week 10 Phase 2 COMPLETE ✅ | Phase 3 NEXT
**Date**: November 19, 2025

---

## Current Status

### ✅ Completed (Weeks 8-9, Week 10 Phases 1-2)
- Week 8: Syscall infrastructure, preemptive scheduling, Ring 3 transition
- Week 9 Phase 1: VFS integration, file operations
- Week 9 Phase 2: Process management (fork, wait, getppid)
- Week 9 Phase 3: Advanced FD ops (pipe, dup, dup2, fcntl)
- Week 10 Phase 1: execve, ELF loading, stack setup (~930 LOC)
- Week 10 Phase 2: POSIX signals, signal syscalls (~1,200 LOC)

**Total Completed**: ~8,500 LOC across 9 phases

---

## Week 10 Phase 3: Shell & Testing (~1,600 LOC)

### Objectives
1. Minimal XINIM shell with job control
2. Process groups and sessions
3. TTY signal integration
4. Comprehensive signal testing
5. Scheduler signal delivery integration

### Implementation Breakdown

**A. Scheduler Signal Integration (150 LOC)**
- File: `src/kernel/scheduler.cpp` (modifications)
- Add `deliver_pending_signals()` before user return
- Add `init_signal_state()` to `server_spawn.cpp`
- Test signal delivery timing

**B. Process Groups & Sessions (300 LOC)**
- File: `src/kernel/process_group.hpp` (80 LOC)
- File: `src/kernel/process_group.cpp` (220 LOC)
- Structures: ProcessGroup, Session
- Syscalls: setpgid, getpgid, setsid, getsid
- Update PCB with pgid, sid fields

**C. TTY Signal Delivery (200 LOC)**
- File: `src/kernel/tty_signals.cpp` (200 LOC)
- Ctrl+C → SIGINT to foreground process group
- Ctrl+Z → SIGTSTP to foreground process group
- Background read/write → SIGTTIN/SIGTTOU

**D. Minimal Shell (600 LOC)**
- Dir: `userland/shell/xinim-sh/`
- File: `shell.cpp` (300 LOC) - Main loop, readline, command execution
- File: `job_control.cpp` (150 LOC) - Job management (bg, fg, jobs)
- File: `builtins.cpp` (100 LOC) - cd, exit, export, etc.
- File: `parser.cpp` (50 LOC) - Simple command parsing

**E. Signal Testing Suite (350 LOC)**
- Dir: `tests/signal/`
- File: `test_signal_basic.cpp` (100 LOC) - Basic send/receive
- File: `test_signal_handlers.cpp` (100 LOC) - Handler execution
- File: `test_signal_masking.cpp` (100 LOC) - Blocking/unblocking
- File: `test_signal_integration.cpp` (50 LOC) - Integration tests

**Estimated Effort**: 12-14 hours

---

## Week 11 Phase 1: Memory Mapping (~1,500 LOC)

### Objectives
1. Virtual Memory Areas (VMA) management
2. mmap/munmap/mprotect/msync syscalls
3. Anonymous and file-backed mappings
4. Page fault handler integration
5. Lazy allocation and copy-on-write prep

### Implementation Breakdown

**A. VMA Structures (200 LOC)**
- File: `src/kernel/vma.hpp` (120 LOC)
- File: `src/kernel/vma.cpp` (80 LOC)
- Structures: VMA, VMAList
- Operations: insert, remove, find, split, merge

**B. Page Fault Handler (250 LOC)**
- File: `src/kernel/page_fault.cpp` (250 LOC)
- Demand paging for anonymous mappings
- File-backed page loading
- Protection violation handling
- Integration with IDT

**C. Memory Mapping Syscalls (450 LOC)**
- File: `src/kernel/syscalls/mmap.cpp` (450 LOC)
- sys_mmap (anonymous, file-backed, shared, private)
- sys_munmap (unmap and free pages)
- sys_mprotect (change protection)
- sys_msync (flush to disk)
- sys_brk (heap management via VMA)

**D. Address Space Management (350 LOC)**
- File: `src/kernel/address_space.hpp` (100 LOC)
- File: `src/kernel/address_space.cpp` (250 LOC)
- Per-process address space descriptor
- VMA tree management
- Page table integration
- execve cleanup (free old VMAs)

**E. Testing & Integration (250 LOC)**
- File: `tests/mmap/test_mmap.cpp` (150 LOC)
- File: `tests/mmap/test_page_fault.cpp` (100 LOC)
- Update execve to use VMA for cleanup
- Update fork to copy VMAs

**Estimated Effort**: 16-18 hours

---

## Week 11 Phase 2: Shared Memory IPC (~1,200 LOC)

### Objectives
1. POSIX shared memory objects (shm_open, shm_unlink)
2. System V shared memory (shmget, shmat, shmdt, shmctl)
3. Memory-mapped IPC regions
4. Reference counting and lifecycle management

### Implementation Breakdown

**A. POSIX Shared Memory (400 LOC)**
- File: `src/kernel/posix_shm.hpp` (100 LOC)
- File: `src/kernel/posix_shm.cpp` (200 LOC)
- File: `src/kernel/syscalls/shm_posix.cpp` (100 LOC)
- sys_shm_open, sys_shm_unlink
- Named shared memory objects
- Reference counting

**B. System V Shared Memory (500 LOC)**
- File: `src/kernel/sysv_shm.hpp` (120 LOC)
- File: `src/kernel/sysv_shm.cpp` (250 LOC)
- File: `src/kernel/syscalls/shm_sysv.cpp` (130 LOC)
- sys_shmget, sys_shmat, sys_shmdt, sys_shmctl
- IPC key management
- Attach/detach tracking

**C. IPC Namespace (150 LOC)**
- File: `src/kernel/ipc_namespace.hpp` (50 LOC)
- File: `src/kernel/ipc_namespace.cpp` (100 LOC)
- IPC key→ID mapping
- Permission checking
- Cleanup on process exit

**D. Testing (150 LOC)**
- File: `tests/shm/test_posix_shm.cpp` (75 LOC)
- File: `tests/shm/test_sysv_shm.cpp` (75 LOC)

**Estimated Effort**: 14-16 hours

---

## Week 11 Phase 3: Advanced IPC (~900 LOC)

### Objectives
1. Message queues (POSIX and System V)
2. Semaphores (POSIX and System V)
3. Robust IPC cleanup on process exit
4. IPC resource limits

### Implementation Breakdown

**A. Message Queues (400 LOC)**
- File: `src/kernel/mqueue.hpp` (100 LOC)
- File: `src/kernel/mqueue.cpp` (200 LOC)
- File: `src/kernel/syscalls/mqueue.cpp` (100 LOC)
- POSIX: mq_open, mq_send, mq_receive, mq_close, mq_unlink
- Priority-based message delivery

**B. Semaphores (350 LOC)**
- File: `src/kernel/semaphore.hpp` (80 LOC)
- File: `src/kernel/semaphore.cpp` (170 LOC)
- File: `src/kernel/syscalls/sem.cpp` (100 LOC)
- POSIX: sem_open, sem_wait, sem_post, sem_close
- System V: semget, semop, semctl
- Undo on process exit

**C. IPC Cleanup (100 LOC)**
- File: `src/kernel/ipc_cleanup.cpp` (100 LOC)
- Track per-process IPC resources
- Cleanup on exit/crash
- Reference counting

**D. Testing (50 LOC)**
- File: `tests/ipc/test_mqueue.cpp` (25 LOC)
- File: `tests/ipc/test_semaphore.cpp` (25 LOC)

**Estimated Effort**: 10-12 hours

---

## Granular Execution Plan

### Week 10 Phase 3 (Next - 12 steps)

1. ✅ Create roadmap documents
2. Modify scheduler.cpp - add signal delivery
3. Modify server_spawn.cpp - init signal state
4. Create process_group.hpp
5. Implement process_group.cpp
6. Add process group syscalls
7. Implement tty_signals.cpp
8. Create xinim-sh shell structure
9. Implement shell main loop
10. Implement job control
11. Create signal test suite
12. Test and document Phase 3

### Week 11 Phase 1 (15 steps)

1. Create vma.hpp structures
2. Implement vma.cpp operations
3. Create page_fault.cpp handler
4. Implement sys_mmap (anonymous)
5. Implement sys_mmap (file-backed)
6. Implement sys_munmap
7. Implement sys_mprotect
8. Implement sys_msync
9. Create address_space.hpp
10. Implement address_space.cpp
11. Update execve for VMA cleanup
12. Update fork for VMA copy
13. Create mmap test suite
14. Integration testing
15. Document Phase 1

### Week 11 Phase 2 (12 steps)

1. Create posix_shm.hpp
2. Implement posix_shm.cpp
3. Add shm_open/shm_unlink syscalls
4. Create sysv_shm.hpp
5. Implement sysv_shm.cpp
6. Add shmget/shmat/shmdt/shmctl syscalls
7. Create ipc_namespace.hpp
8. Implement ipc_namespace.cpp
9. Add IPC cleanup hooks
10. Create shm test suite
11. Integration testing
12. Document Phase 2

### Week 11 Phase 3 (10 steps)

1. Create mqueue.hpp
2. Implement mqueue.cpp + syscalls
3. Create semaphore.hpp
4. Implement semaphore.cpp + syscalls
5. Create ipc_cleanup.cpp
6. Add exit hooks
7. Create IPC test suite
8. Integration testing
9. Document Phase 3
10. Week 11 complete documentation

---

## Summary

**Total Remaining Work**: ~5,200 LOC across 7 phases

**Week 10 Phase 3**: ~1,600 LOC (12-14 hours)
**Week 11 Phase 1**: ~1,500 LOC (16-18 hours)
**Week 11 Phase 2**: ~1,200 LOC (14-16 hours)
**Week 11 Phase 3**: ~900 LOC (10-12 hours)

**Total Estimated Effort**: 52-60 hours

**Granular Steps**: 49 discrete implementation steps

Each step will be tracked with fine-grained TODO list updates.

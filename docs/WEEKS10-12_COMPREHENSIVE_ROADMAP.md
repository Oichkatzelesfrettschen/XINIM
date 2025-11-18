# Weeks 10-12: Process Execution, Advanced IPC & System Integration - Comprehensive Roadmap

**Date**: November 18, 2025
**Status**: 🚀 READY TO START
**Branch**: `claude/audit-translate-repo-018NNYQuW6XJrQJL2LLUPS45`
**Estimated Effort**: 60-80 hours total (20-30 hours per week)

---

## Executive Summary

Weeks 10-12 complete the XINIM microkernel implementation by adding:
- **Week 10**: Program execution (execve), signal handling, shell integration
- **Week 11**: Advanced memory management (mmap, shared memory), enhanced IPC
- **Week 12**: Security hardening, performance optimization, comprehensive testing

By the end of Week 12, XINIM will be a **fully functional POSIX-compliant microkernel** capable of running real userspace applications, shells, and multi-process pipelines.

---

## Strategic Overview

### Current State (End of Week 9)
✅ Preemptive scheduling with timer interrupts
✅ Ring 3 user mode with syscall/sysret
✅ VFS integration with file descriptors
✅ Process management (fork, wait, exit)
✅ Advanced FD operations (dup, dup2, fcntl)
✅ Pipes for IPC with blocking I/O

### Missing Critical Features
❌ execve (cannot execute new programs)
❌ Signal handling (cannot deliver signals)
❌ Memory mapping (mmap/munmap)
❌ Shared memory IPC
❌ Security mechanisms (capabilities, sandboxing)
❌ Performance optimization
❌ Comprehensive testing

---

## Week 10: Program Execution & Signal Handling

**Goal**: Enable execution of arbitrary programs and deliver signals

### Week 10 Phase 1: execve Implementation (8-10 hours)

#### Objectives
- Load ELF binaries into process address space
- Replace current process image with new program
- Preserve open file descriptors (except CLOEXEC)
- Set up initial stack with argc/argv/envp

#### Key Components

**1. ELF Loader Enhancement** (`src/kernel/elf_loader.cpp` - 400 LOC)
```cpp
/**
 * @brief Load ELF binary and prepare for execution
 *
 * Steps:
 * 1. Parse ELF headers (validate magic, architecture)
 * 2. Load program segments into memory
 * 3. Set up BSS section (zero-initialized data)
 * 4. Map dynamic linker if PT_INTERP present
 * 5. Return entry point address
 */
int load_elf_binary(const char* path,
                    uint64_t* entry_point,
                    uint64_t* stack_top);
```

**2. sys_execve Implementation** (`src/kernel/syscalls/exec.cpp` - 350 LOC)
```cpp
/**
 * @brief Execute new program
 *
 * POSIX: int execve(const char *pathname,
 *                   char *const argv[],
 *                   char *const envp[])
 *
 * Process:
 * 1. Validate pathname, argv, envp (copy from user space)
 * 2. Open executable file
 * 3. Parse and load ELF binary
 * 4. Free old address space (except kernel stack)
 * 5. Close FDs with CLOEXEC flag set
 * 6. Set up new stack with argc/argv/envp
 * 7. Reset signal handlers to default
 * 8. Jump to entry point (does not return)
 */
int64_t sys_execve(uint64_t pathname_addr,
                   uint64_t argv_addr,
                   uint64_t envp_addr);
```

**3. Stack Setup** (`src/kernel/exec_stack.cpp` - 200 LOC)
```cpp
/**
 * @brief Set up initial user stack for new program
 *
 * Stack layout (grows downward):
 * ┌─────────────────┐ ← Top of stack
 * │ NULL            │ (envp terminator)
 * │ env strings     │
 * │ NULL            │ (argv terminator)
 * │ arg strings     │
 * │ envp pointers   │
 * │ argv pointers   │
 * │ argc            │
 * └─────────────────┘ ← RSP points here
 */
uint64_t setup_exec_stack(char* argv[], char* envp[]);
```

#### Files Created
- `src/kernel/elf_loader.cpp` (400 LOC) - ELF parsing and loading
- `src/kernel/elf_loader.hpp` (80 LOC) - ELF structures
- `src/kernel/syscalls/exec.cpp` (350 LOC) - sys_execve implementation
- `src/kernel/exec_stack.cpp` (200 LOC) - Stack initialization
- `docs/WEEK10_PHASE1_EXECVE_PLAN.md` (500 LOC) - Detailed plan

#### Testing
- Load and execute `/bin/test_hello` (prints "Hello, World!")
- Verify argc/argv/envp passed correctly
- Test CLOEXEC flag handling
- Verify old address space cleanup

---

### Week 10 Phase 2: Signal Framework (10-12 hours)

#### Objectives
- Implement signal delivery mechanism
- Support signal handlers (sigaction)
- Handle signal masks and blocking
- Implement common signals (SIGCHLD, SIGPIPE, SIGINT, SIGTERM)

#### Key Components

**1. Signal Data Structures** (`src/kernel/signal.hpp` - 200 LOC)
```cpp
/**
 * @brief Signal handler types
 */
enum class SignalAction {
    DEFAULT,   // Default action
    IGNORE,    // Ignore signal
    HANDLER    // User-defined handler
};

/**
 * @brief Signal handler entry
 */
struct SignalHandler {
    SignalAction action;
    uint64_t handler_addr;  // User-space handler function
    uint64_t flags;         // SA_RESTART, SA_SIGINFO, etc.
    uint64_t mask;          // Signals blocked during handler
};

/**
 * @brief Process signal state
 */
struct SignalState {
    SignalHandler handlers[32];  // Signal handlers (1-31)
    uint64_t pending;            // Pending signals bitmask
    uint64_t blocked;            // Blocked signals bitmask
    bool in_handler;             // Currently executing handler
};
```

**2. Signal Delivery** (`src/kernel/signal.cpp` - 400 LOC)
```cpp
/**
 * @brief Send signal to process
 *
 * Steps:
 * 1. Check if signal is blocked
 * 2. Set pending bit
 * 3. Wake process if blocked
 * 4. Deliver on next kernel→user transition
 */
int send_signal(ProcessControlBlock* pcb, int signum);

/**
 * @brief Deliver pending signals
 *
 * Called before returning to user mode.
 * Sets up signal trampoline on user stack.
 */
void deliver_pending_signals(ProcessControlBlock* pcb);

/**
 * @brief Return from signal handler
 *
 * Restores original user context.
 */
void signal_return(ProcessControlBlock* pcb);
```

**3. Signal Syscalls** (`src/kernel/syscalls/signal.cpp` - 350 LOC)
```cpp
// sys_kill (37): Send signal to process
int64_t sys_kill(uint64_t pid, uint64_t sig);

// sys_sigaction (13): Set signal handler
int64_t sys_sigaction(uint64_t signum,
                      uint64_t new_action_addr,
                      uint64_t old_action_addr);

// sys_sigprocmask (14): Manipulate signal mask
int64_t sys_sigprocmask(uint64_t how,
                        uint64_t set_addr,
                        uint64_t oldset_addr);

// sys_sigreturn (15): Return from signal handler
int64_t sys_sigreturn();
```

**4. Signal Trampoline** (`src/arch/x86_64/signal_trampoline.S` - 100 LOC)
```asm
/**
 * @brief Signal handler wrapper
 *
 * Automatically injected on user stack when signal delivered.
 * Calls user handler, then invokes sys_sigreturn.
 */
signal_trampoline:
    # Call user signal handler
    call *%rax

    # Return from signal
    mov $15, %rax        # sys_sigreturn
    syscall

    # Should never reach here
    ud2
```

#### Files Created
- `src/kernel/signal.hpp` (200 LOC) - Signal data structures
- `src/kernel/signal.cpp` (400 LOC) - Signal delivery mechanism
- `src/kernel/syscalls/signal.cpp` (350 LOC) - Signal syscalls
- `src/arch/x86_64/signal_trampoline.S` (100 LOC) - Signal trampoline
- `docs/WEEK10_PHASE2_SIGNAL_PLAN.md` (600 LOC) - Detailed plan

#### Testing
- Test SIGCHLD delivery on child exit
- Test SIGPIPE on pipe write with closed read end
- Test custom signal handlers with sigaction
- Test signal masking and blocking

---

### Week 10 Phase 3: Shell Integration & Testing (6-8 hours)

#### Objectives
- Create simple test shell
- Test full pipeline execution (`cmd1 | cmd2 | cmd3`)
- Verify I/O redirection (`>`, `<`, `2>`)
- End-to-end integration testing

#### Key Components

**1. Simple Test Shell** (`userland/test_shell.c` - 500 LOC)
```c
/**
 * @brief Minimal shell for testing
 *
 * Features:
 * - Command execution via fork+execve
 * - Pipeline support (cmd1 | cmd2)
 * - I/O redirection (>, <)
 * - Background execution (&)
 * - Built-in commands (cd, exit)
 */
int main() {
    while (1) {
        printf("xinim> ");
        char* line = read_line();
        execute_command(line);
    }
}
```

**2. Test Programs** (`userland/test_programs/`)
- `test_hello.c` - Print "Hello, World!"
- `test_cat.c` - Read from stdin, write to stdout
- `test_grep.c` - Simple pattern matching
- `test_pipe.c` - Test pipe communication
- `test_signal.c` - Test signal handling

**3. Integration Tests** (`test/week10_integration.cpp` - 400 LOC)
```cpp
// Test 1: Simple execve
TEST(Week10, SimpleExecve) {
    if (fork() == 0) {
        execve("/bin/test_hello", {"test_hello"}, {});
    }
    wait4(-1, &status, 0, nullptr);
    EXPECT_EQ(status, 0);
}

// Test 2: Pipeline
TEST(Week10, Pipeline) {
    // echo "hello" | grep "ell"
    int pipe1[2];
    pipe(pipe1);

    if (fork() == 0) {
        dup2(pipe1[1], 1);
        close(pipe1[0]);
        execve("/bin/echo", {"echo", "hello"}, {});
    }

    if (fork() == 0) {
        dup2(pipe1[0], 0);
        close(pipe1[1]);
        execve("/bin/grep", {"grep", "ell"}, {});
    }

    close(pipe1[0]);
    close(pipe1[1]);
    wait4(-1, &status, 0, nullptr);
    wait4(-1, &status, 0, nullptr);
}

// Test 3: Signal handling
TEST(Week10, SignalHandling) {
    // Test SIGCHLD delivery on child exit
    // Test SIGPIPE on broken pipe
}
```

#### Files Created
- `userland/test_shell.c` (500 LOC) - Test shell
- `userland/test_programs/test_*.c` (5 files, ~100 LOC each)
- `test/week10_integration.cpp` (400 LOC) - Integration tests
- `docs/WEEK10_PHASE3_TESTING_PLAN.md` (400 LOC) - Testing strategy
- `docs/WEEK10_COMPLETE.md` (600 LOC) - Completion report

#### Testing
- Execute simple commands via shell
- Test multi-stage pipelines
- Test I/O redirection
- Test signal delivery in shell

---

## Week 11: Advanced Memory Management & IPC

**Goal**: Implement memory mapping and advanced IPC mechanisms

### Week 11 Phase 1: Memory Mapping (10-12 hours)

#### Objectives
- Implement mmap/munmap syscalls
- Support file-backed and anonymous mappings
- Implement memory protection flags
- Enable shared memory regions

#### Key Components

**1. VMA (Virtual Memory Area) Management** (`src/kernel/vma.hpp` - 200 LOC)
```cpp
/**
 * @brief Virtual Memory Area
 *
 * Represents a contiguous virtual memory region.
 */
struct VMA {
    uint64_t start;         // Start address
    uint64_t end;           // End address
    uint32_t prot;          // Protection flags (PROT_READ, PROT_WRITE, PROT_EXEC)
    uint32_t flags;         // MAP_PRIVATE, MAP_SHARED, MAP_ANONYMOUS
    void* inode;            // Backing file (nullptr for anonymous)
    uint64_t offset;        // File offset
    VMA* next;              // Next VMA in list
};

/**
 * @brief VMA list for process
 */
struct VMAList {
    VMA* head;

    VMA* find_vma(uint64_t addr);
    int insert_vma(VMA* vma);
    int remove_vma(uint64_t start, uint64_t end);
    int split_vma(uint64_t addr);
};
```

**2. mmap/munmap Implementation** (`src/kernel/syscalls/mmap.cpp` - 450 LOC)
```cpp
/**
 * @brief Map memory region
 *
 * POSIX: void *mmap(void *addr, size_t length, int prot,
 *                   int flags, int fd, off_t offset)
 */
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, uint64_t fd, uint64_t offset);

/**
 * @brief Unmap memory region
 *
 * POSIX: int munmap(void *addr, size_t length)
 */
int64_t sys_munmap(uint64_t addr, uint64_t length);

/**
 * @brief Change memory protection
 *
 * POSIX: int mprotect(void *addr, size_t len, int prot)
 */
int64_t sys_mprotect(uint64_t addr, uint64_t length, uint64_t prot);
```

**3. Page Fault Handler Updates** (`src/kernel/page_fault.cpp` - 300 LOC)
```cpp
/**
 * @brief Handle page fault
 *
 * Steps:
 * 1. Find VMA containing faulting address
 * 2. Check if access is allowed (read/write/exec)
 * 3. Allocate physical page if not present
 * 4. Load from file if file-backed
 * 5. Install page table entry
 */
void handle_page_fault(uint64_t fault_addr, uint64_t error_code);
```

#### Files Created
- `src/kernel/vma.hpp` (200 LOC) - VMA structures
- `src/kernel/vma.cpp` (350 LOC) - VMA management
- `src/kernel/syscalls/mmap.cpp` (450 LOC) - mmap/munmap/mprotect
- `src/kernel/page_fault.cpp` (300 LOC) - Enhanced page fault handler
- `docs/WEEK11_PHASE1_MMAP_PLAN.md` (500 LOC) - Detailed plan

#### Testing
- Test anonymous mmap (heap allocation)
- Test file-backed mmap (memory-mapped files)
- Test memory protection (PROT_READ, PROT_WRITE, PROT_EXEC)
- Test munmap and partial unmapping

---

### Week 11 Phase 2: Shared Memory IPC (8-10 hours)

#### Objectives
- Implement System V shared memory (shmget, shmat, shmdt, shmctl)
- Support POSIX shared memory (shm_open, shm_unlink)
- Enable zero-copy IPC between processes

#### Key Components

**1. Shared Memory Segments** (`src/kernel/shm.hpp` - 200 LOC)
```cpp
/**
 * @brief Shared memory segment
 */
struct ShmSegment {
    int shmid;              // Shared memory ID
    size_t size;            // Segment size
    uint32_t perm;          // Permissions
    pid_t creator_pid;      // Creator process
    int attach_count;       // Number of attachments
    uint64_t phys_addr;     // Physical address
    ShmSegment* next;       // Next segment
};
```

**2. Shared Memory Syscalls** (`src/kernel/syscalls/shm.cpp` - 500 LOC)
```cpp
// System V shared memory
int64_t sys_shmget(uint64_t key, uint64_t size, uint64_t shmflg);
int64_t sys_shmat(uint64_t shmid, uint64_t shmaddr, uint64_t shmflg);
int64_t sys_shmdt(uint64_t shmaddr);
int64_t sys_shmctl(uint64_t shmid, uint64_t cmd, uint64_t buf);
```

#### Files Created
- `src/kernel/shm.hpp` (200 LOC) - Shared memory structures
- `src/kernel/shm.cpp` (400 LOC) - Shared memory management
- `src/kernel/syscalls/shm.cpp` (500 LOC) - Shared memory syscalls
- `docs/WEEK11_PHASE2_SHM_PLAN.md` (450 LOC) - Detailed plan

#### Testing
- Test shared memory creation and attachment
- Test zero-copy data transfer between processes
- Test shared memory permissions
- Test cleanup on process exit

---

### Week 11 Phase 3: Advanced IPC Mechanisms (6-8 hours)

#### Objectives
- Implement POSIX message queues
- Implement semaphores (System V and POSIX)
- Performance optimization for IPC

#### Key Components

**1. Message Queues** (`src/kernel/mqueue.cpp` - 400 LOC)
```cpp
int64_t sys_mq_open(uint64_t name, uint64_t oflag, uint64_t mode);
int64_t sys_mq_send(uint64_t mqdes, uint64_t msg_ptr, uint64_t msg_len);
int64_t sys_mq_receive(uint64_t mqdes, uint64_t msg_ptr, uint64_t msg_len);
int64_t sys_mq_close(uint64_t mqdes);
int64_t sys_mq_unlink(uint64_t name);
```

**2. Semaphores** (`src/kernel/sem.cpp` - 350 LOC)
```cpp
// POSIX semaphores
int64_t sys_sem_open(uint64_t name, uint64_t oflag, uint64_t mode, uint64_t value);
int64_t sys_sem_wait(uint64_t sem);
int64_t sys_sem_post(uint64_t sem);
int64_t sys_sem_close(uint64_t sem);
```

#### Files Created
- `src/kernel/mqueue.cpp` (400 LOC) - Message queues
- `src/kernel/sem.cpp` (350 LOC) - Semaphores
- `docs/WEEK11_PHASE3_IPC_PLAN.md` (400 LOC) - Detailed plan
- `docs/WEEK11_COMPLETE.md` (550 LOC) - Completion report

---

## Week 12: Security, Optimization & Testing

**Goal**: Harden system, optimize performance, comprehensive testing

### Week 12 Phase 1: Security Hardening (10-12 hours)

#### Objectives
- Implement capability-based security
- Add syscall argument validation
- Implement ASLR (Address Space Layout Randomization)
- Add stack canaries and overflow protection

#### Key Components

**1. Capability System** (`src/kernel/capabilities.cpp` - 400 LOC)
```cpp
/**
 * @brief Process capabilities
 *
 * Based on Linux capabilities (cap_t).
 */
struct Capabilities {
    uint64_t effective;     // Currently effective caps
    uint64_t permitted;     // Maximum allowed caps
    uint64_t inheritable;   // Inherited across execve

    bool has_cap(int cap);
    void drop_cap(int cap);
    void raise_cap(int cap);
};

// Capability types
#define CAP_SYS_ADMIN   0   // System administration
#define CAP_NET_ADMIN   1   // Network administration
#define CAP_SYS_PTRACE  2   // Trace processes
#define CAP_KILL        3   // Send signals
```

**2. ASLR Implementation** (`src/kernel/aslr.cpp` - 250 LOC)
```cpp
/**
 * @brief Randomize address space layout
 *
 * Randomizes:
 * - Stack base
 * - Heap base
 * - mmap regions
 * - Shared libraries
 */
void randomize_address_space(ProcessControlBlock* pcb);
```

**3. Argument Validation** (`src/kernel/syscall_validation.cpp` - 300 LOC)
```cpp
/**
 * @brief Validate syscall arguments
 *
 * Checks:
 * - Pointer validity
 * - Buffer overflow
 * - Integer overflow
 * - Permission checks
 */
bool validate_syscall_args(uint64_t syscall_num, ...);
```

#### Files Created
- `src/kernel/capabilities.hpp` (150 LOC)
- `src/kernel/capabilities.cpp` (400 LOC)
- `src/kernel/aslr.cpp` (250 LOC)
- `src/kernel/syscall_validation.cpp` (300 LOC)
- `docs/WEEK12_PHASE1_SECURITY_PLAN.md` (500 LOC)

---

### Week 12 Phase 2: Performance Optimization (8-10 hours)

#### Objectives
- Optimize scheduler (O(1) scheduling)
- Optimize memory allocator
- Optimize IPC fast path
- Profile and identify bottlenecks

#### Key Components

**1. O(1) Scheduler** (`src/kernel/scheduler_o1.cpp` - 400 LOC)
```cpp
/**
 * @brief O(1) priority-based scheduler
 *
 * Uses priority queues for constant-time scheduling.
 */
class O1Scheduler {
    ProcessQueue active[MAX_PRIORITIES];
    ProcessQueue expired[MAX_PRIORITIES];

    ProcessControlBlock* pick_next_task();
    void enqueue_task(ProcessControlBlock* pcb);
    void dequeue_task(ProcessControlBlock* pcb);
};
```

**2. Slab Allocator** (`src/kernel/slab.cpp` - 500 LOC)
```cpp
/**
 * @brief Slab allocator for kernel objects
 *
 * Pre-allocates common sizes for fast allocation.
 */
class SlabAllocator {
    Slab* slabs[16];  // 16, 32, 64, ..., 4096 bytes

    void* alloc(size_t size);
    void free(void* ptr);
};
```

**3. Fast IPC Path** (`src/kernel/ipc_fastpath.cpp` - 300 LOC)
```cpp
/**
 * @brief Optimized IPC for small messages
 *
 * Avoids memory copies for messages < 64 bytes.
 */
int fast_ipc_send(pid_t dest, const void* msg, size_t len);
```

#### Files Created
- `src/kernel/scheduler_o1.cpp` (400 LOC)
- `src/kernel/slab.cpp` (500 LOC)
- `src/kernel/ipc_fastpath.cpp` (300 LOC)
- `docs/WEEK12_PHASE2_OPTIMIZATION_PLAN.md` (450 LOC)

---

### Week 12 Phase 3: Comprehensive Testing (8-10 hours)

#### Objectives
- Create full test suite
- Stress testing and fuzzing
- Performance benchmarking
- Documentation and final report

#### Key Components

**1. Test Suite** (`test/comprehensive_suite.cpp` - 1000 LOC)
- Unit tests for all syscalls
- Integration tests for process lifecycle
- Stress tests for scheduler
- Fuzz tests for syscall arguments
- Performance benchmarks

**2. Benchmarks** (`test/benchmarks/` - 500 LOC)
- Context switch latency
- IPC throughput
- Fork/exec performance
- Memory allocation speed

**3. Final Documentation** (`docs/WEEK12_COMPLETE.md` - 800 LOC)
- System architecture overview
- Performance characteristics
- Known limitations
- Future roadmap

#### Files Created
- `test/comprehensive_suite.cpp` (1000 LOC)
- `test/benchmarks/*.cpp` (500 LOC)
- `docs/WEEK12_PHASE3_TESTING_PLAN.md` (500 LOC)
- `docs/WEEK12_COMPLETE.md` (800 LOC)
- `docs/XINIM_FINAL_REPORT.md` (1500 LOC)

---

## Implementation Strategy

### Execution Order
```
Week 10 Phase 1 → Week 10 Phase 2 → Week 10 Phase 3
                                        ↓
Week 11 Phase 1 → Week 11 Phase 2 → Week 11 Phase 3
                                        ↓
Week 12 Phase 1 → Week 12 Phase 2 → Week 12 Phase 3
```

### Granular TODO Breakdown
Each phase will be broken into 10-20 granular tasks:
1. Read existing code
2. Design data structures
3. Implement core functionality
4. Add error handling
5. Update syscall table
6. Update build system
7. Write unit tests
8. Write integration tests
9. Document implementation
10. Commit and push

### Testing at Each Phase
- Unit tests after each component
- Integration tests after each phase
- End-to-end tests after each week

---

## Estimated LOC by Week

| Week | New Files | LOC New | LOC Modified | Total |
|------|-----------|---------|--------------|-------|
| 10   | 15        | ~3500   | ~200         | 3700  |
| 11   | 12        | ~3200   | ~150         | 3350  |
| 12   | 15        | ~4500   | ~250         | 4750  |
| **Total** | **42** | **~11200** | **~600** | **11800** |

---

## Success Criteria

### Week 10
✅ execve loads and executes ELF binaries
✅ Signals delivered correctly (SIGCHLD, SIGPIPE, SIGINT)
✅ Shell can execute commands and pipelines
✅ All file descriptors work correctly across exec

### Week 11
✅ mmap/munmap work for anonymous and file-backed memory
✅ Shared memory IPC works between processes
✅ Message queues and semaphores functional
✅ Zero-copy IPC achieves >1GB/s throughput

### Week 12
✅ Capability system restricts privileged operations
✅ ASLR enabled for all processes
✅ O(1) scheduler handles 1000+ processes
✅ All tests pass with 100% coverage
✅ Performance benchmarks meet targets

---

## Risk Mitigation

### Technical Risks
1. **ELF loader complexity**: Start with static binaries, add dynamic linking in Week 11
2. **Signal delivery race conditions**: Use signal queues, disable interrupts during delivery
3. **Memory mapping bugs**: Extensive testing with various mmap patterns
4. **Performance regressions**: Continuous benchmarking after each change

### Schedule Risks
1. **Underestimated complexity**: Each phase has 20% time buffer
2. **Blocking issues**: Phases are designed to be independently testable
3. **Integration problems**: Integration testing after each phase

---

## Next Steps

1. ✅ Create comprehensive roadmap (this document)
2. ⏭️ Create Week 10 Phase 1 detailed plan
3. ⏭️ Implement execve step-by-step
4. ⏭️ Continue with Week 10 Phases 2-3
5. ⏭️ Proceed to Weeks 11-12

---

**Weeks 10-12 Roadmap Complete**
**Ready to Begin Week 10 Phase 1: execve Implementation**

Total Estimated Effort: 60-80 hours
Total Estimated LOC: ~11,800
Completion Target: End of November 2025

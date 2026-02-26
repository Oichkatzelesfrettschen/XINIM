# Week 9 Phase 2: Process Management Syscalls

**Status:** 📋 **PLANNING**
**Date:** November 18, 2025
**Milestone:** Implement POSIX process management (fork, wait, getppid)

---

## Executive Summary

Week 9 Phase 2 implements the core POSIX process management syscalls, enabling userspace processes to create children, wait for their termination, and manage the process tree. This establishes the foundation for shell job control, daemon processes, and multi-process applications.

**Goal:** Transform XINIM from single-process servers to full multi-process capability with proper parent-child relationships and lifecycle management.

---

## Prerequisites (Completed in Phase 1)

- ✅ File descriptor tables (per-process)
- ✅ VFS integration
- ✅ User pointer validation
- ✅ Process exit (sys_exit marks ZOMBIE)
- ✅ Scheduler with preemptive multitasking

---

## Week 9 Phase 2 Objectives

### Primary Goals

1. **Parent-Child Relationships** - Track process hierarchy
2. **sys_getppid** - Get parent process ID
3. **sys_fork** - Create child process (copy of parent)
4. **sys_wait4** - Wait for child to exit
5. **Process Cleanup** - Reap zombie processes

### Success Criteria

- [ ] Parent can fork multiple children
- [ ] Children inherit FD table from parent
- [ ] Parent can wait for children to exit
- [ ] Zombie processes are properly reaped
- [ ] Orphaned processes are handled
- [ ] No memory leaks in process lifecycle

---

## Technical Architecture

### Process Lifecycle State Machine

```
                    ┌──────────┐
                    │  CREATED │
                    └────┬─────┘
                         │ spawn()
                         ↓
    fork() ────────→ ┌──────┐
                     │ READY │ ←──────┐
                     └───┬──┘         │
                         │ schedule() │
                         ↓            │
                     ┌─────────┐      │
                     │ RUNNING │      │
                     └────┬────┘      │
                          │           │
                ┌─────────┼─────────┐ │
                │         │         │ │
             exit()    block()   yield()
                │         │         │
                ↓         ↓         └─┘
            ┌────────┐ ┌─────────┐
            │ ZOMBIE │ │ BLOCKED │
            └────┬───┘ └────┬────┘
                 │          │ unblock()
            wait4()         └──────────┘
                 │
                 ↓
            ┌────────┐
            │  DEAD  │ (freed)
            └────────┘
```

### Parent-Child Relationship Tree

```
                PID 1 (init)
                   │
         ┌─────────┼─────────┐
         │         │         │
       PID 2     PID 3     PID 4
    (VFS Server) (Proc)   (Mem)
         │
    ┌────┴────┐
    │         │
  PID 5    PID 6
  (child1) (child2)
```

---

## Implementation Plan

### Step 1: Add Parent-Child Tracking to PCB

**Why First?** Need to track relationships before implementing fork/wait.

#### Modifications to PCB

```cpp
// In src/kernel/pcb.hpp

struct ProcessControlBlock {
    // ... existing fields ...

    // Week 9 Phase 2: Process hierarchy
    xinim::pid_t parent_pid;        ///< Parent process PID (0 for orphans)

    // Children list (linked list of child PIDs)
    struct ChildNode {
        xinim::pid_t pid;
        ChildNode* next;
    };
    ChildNode* children_head;       ///< Head of children list

    // Zombie handling
    bool has_exited;                ///< Has this process called exit()?
    bool has_been_waited;           ///< Has parent called wait() on this?

    // ... existing fields ...
};
```

**Key Design Decisions:**
- Parent PID = 0 means orphaned (or init has no parent)
- Children list allows parent to iterate children
- Separate flags for exit vs wait (zombie state)

---

### Step 2: Implement sys_getppid (Warmup)

**Simplest syscall** - just return parent_pid.

```cpp
/**
 * @brief Get parent process ID
 *
 * POSIX: pid_t getppid(void)
 *
 * @return Parent PID, or 0 if orphaned
 */
extern "C" int64_t sys_getppid(uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    return (int64_t)current->parent_pid;
}
```

**Testing:**
```c
pid_t ppid = getppid();
printf("My parent is PID %d\n", ppid);
```

---

### Step 3: Implement sys_fork (Core Complexity)

**Most complex syscall in Phase 2.** Creates exact copy of calling process.

#### Fork Algorithm

1. **Allocate child PCB**
2. **Copy process state:**
   - Copy CPU context
   - Copy FD table (shallow copy, share inodes)
   - Copy stack (deep copy)
3. **Set up parent-child relationship:**
   - Set child->parent_pid = parent->pid
   - Add child to parent->children_head list
4. **Differentiate return values:**
   - Parent returns child PID
   - Child returns 0
5. **Add child to scheduler**

#### Implementation

```cpp
/**
 * @brief Create child process (copy of current)
 *
 * POSIX: pid_t fork(void)
 *
 * @return In parent: child PID
 *         In child: 0
 *         On error: negative error code
 */
extern "C" int64_t sys_fork(uint64_t, uint64_t, uint64_t,
                             uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* parent = get_current_process();
    if (!parent) return -ESRCH;

    // 1. Allocate child PCB
    ProcessControlBlock* child = allocate_process();
    if (!child) return -ENOMEM;

    // 2. Copy parent state
    child->name = parent->name;
    child->state = ProcessState::READY;
    child->priority = parent->priority;

    // 3. Copy CPU context
    child->context = parent->context;

    // 4. Set child return value to 0 (parent returns child PID)
    child->context.rax = 0;  // Child sees fork() return 0

    // 5. Copy stack
    child->stack_size = parent->stack_size;
    child->stack_base = kmalloc(child->stack_size);
    if (!child->stack_base) {
        free_process(child);
        return -ENOMEM;
    }
    memcpy(child->stack_base, parent->stack_base, child->stack_size);

    // 6. Copy kernel stack
    child->kernel_stack_size = parent->kernel_stack_size;
    child->kernel_stack_base = kmalloc(child->kernel_stack_size);
    if (!child->kernel_stack_base) {
        kfree(child->stack_base);
        free_process(child);
        return -ENOMEM;
    }
    memcpy(child->kernel_stack_base, parent->kernel_stack_base,
           child->kernel_stack_size);

    // 7. Copy FD table
    int ret = parent->fd_table.clone_to(&child->fd_table);
    if (ret < 0) {
        kfree(child->kernel_stack_base);
        kfree(child->stack_base);
        free_process(child);
        return ret;
    }

    // 8. Set up parent-child relationship
    child->parent_pid = parent->pid;

    // Add child to parent's children list
    ChildNode* child_node = new ChildNode;
    child_node->pid = child->pid;
    child_node->next = parent->children_head;
    parent->children_head = child_node;

    // 9. Initialize child-specific fields
    child->children_head = nullptr;
    child->has_exited = false;
    child->has_been_waited = false;
    child->exit_status = 0;

    // 10. Add to scheduler
    scheduler_add_process(child);

    // 11. Return child PID to parent
    return (int64_t)child->pid;
}
```

#### Critical Details

**Stack Copying:**
- Must deep copy stack (each process has own stack)
- Stack pointers in context must be adjusted if stack moves

**FD Table Copying:**
- Shallow copy (both processes share same inode)
- Week 10: Add reference counting for inodes

**Return Value Magic:**
- Parent executes normally, returns child PID
- Child starts with RAX=0, so sees fork() return 0

---

### Step 4: Implement sys_wait4 (Blocking & Reaping)

**Purpose:** Parent waits for child to exit and reaps zombie.

#### Wait Algorithm

1. **Find zombie child** (or any child if pid=-1)
2. **If no zombie child exists:**
   - Block parent (set state = BLOCKED, reason = WAIT_CHILD)
   - Scheduler skips blocked processes
3. **If zombie child exists:**
   - Copy exit status
   - Remove child from children list
   - Free child PCB
   - Return child PID

#### Implementation

```cpp
/**
 * @brief Wait for child process to exit
 *
 * POSIX: pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage)
 *
 * Week 9: Simplified - only supports pid=-1 (any child), no options/rusage
 *
 * @param pid_arg Child to wait for (-1 = any child)
 * @param status_addr User space pointer to store exit status (or 0)
 * @param options Wait options (Week 9: ignored, no WNOHANG yet)
 * @return Child PID on success, negative error on failure
 *         -ECHILD: No children exist
 */
extern "C" int64_t sys_wait4(uint64_t pid_arg, uint64_t status_addr,
                              uint64_t options, uint64_t) {
    ProcessControlBlock* parent = get_current_process();
    if (!parent) return -ESRCH;

    int target_pid = (int)pid_arg;

    // Week 9: Only support pid = -1 (any child)
    if (target_pid != -1) {
        return -ENOSYS;  // Specific PID not yet supported
    }

    // Check if parent has any children
    if (!parent->children_head) {
        return -ECHILD;  // No children
    }

    // Search for zombie child
    ChildNode* prev = nullptr;
    ChildNode* curr = parent->children_head;

    while (curr) {
        ProcessControlBlock* child = find_process_by_pid(curr->pid);

        if (child && child->state == ProcessState::ZOMBIE) {
            // Found zombie child!

            // Copy exit status to user space
            if (status_addr != 0) {
                int status = child->exit_status << 8;  // POSIX format
                int ret = copy_to_user(status_addr, &status, sizeof(int));
                if (ret < 0) return ret;
            }

            pid_t child_pid = child->pid;

            // Remove from children list
            if (prev) {
                prev->next = curr->next;
            } else {
                parent->children_head = curr->next;
            }
            delete curr;

            // Free child resources
            free_process_resources(child);
            free_process(child);

            return (int64_t)child_pid;
        }

        prev = curr;
        curr = curr->next;
    }

    // No zombie children found - block parent
    parent->state = ProcessState::BLOCKED;
    parent->blocked_on = BlockReason::WAIT_CHILD;

    // Yield to scheduler (will skip this process until unblocked)
    schedule();

    // When we return here, a child has exited
    // Retry the wait (tail recursion)
    return sys_wait4(pid_arg, status_addr, options, 0);
}
```

#### Wake Parent on Child Exit

**Modify sys_exit:**

```cpp
[[noreturn]] int64_t sys_exit(uint64_t status, uint64_t, uint64_t,
                               uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) {
        for(;;) { __asm__ volatile ("hlt"); }
    }

    // Save exit status
    current->exit_status = (int)status;
    current->has_exited = true;

    // Mark as zombie
    current->state = ProcessState::ZOMBIE;

    // Wake parent if it's waiting
    if (current->parent_pid != 0) {
        ProcessControlBlock* parent = find_process_by_pid(current->parent_pid);
        if (parent && parent->state == ProcessState::BLOCKED &&
            parent->blocked_on == BlockReason::WAIT_CHILD) {
            // Wake parent
            parent->state = ProcessState::READY;
            parent->blocked_on = BlockReason::NONE;
        }
    }

    // Yield to next process
    schedule();

    // Never returns
    for(;;) { __asm__ volatile ("hlt"); }
}
```

---

### Step 5: Process Cleanup & Resource Management

**Challenge:** Need to free process resources when reaped.

#### Free Process Resources

```cpp
void free_process_resources(ProcessControlBlock* pcb) {
    if (!pcb) return;

    // Free stacks
    if (pcb->stack_base) {
        kfree(pcb->stack_base);
        pcb->stack_base = nullptr;
    }

    if (pcb->kernel_stack_base) {
        kfree(pcb->kernel_stack_base);
        pcb->kernel_stack_base = nullptr;
    }

    // Close all open file descriptors
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; i++) {
        if (pcb->fd_table.fds[i].is_open) {
            // Week 10: Decrement inode ref count
            pcb->fd_table.close_fd(i);
        }
    }

    // Free children list
    ChildNode* child = pcb->children_head;
    while (child) {
        ChildNode* next = child->next;
        delete child;
        child = next;
    }
    pcb->children_head = nullptr;
}
```

---

### Step 6: Handle Orphaned Processes

**Problem:** What if parent exits before child?

**Solution:** Reparent orphaned children to init (PID 1)

```cpp
// In sys_exit, before marking as zombie:

// Reparent children to init (PID 1)
ChildNode* child = current->children_head;
while (child) {
    ProcessControlBlock* child_pcb = find_process_by_pid(child->pid);
    if (child_pcb) {
        child_pcb->parent_pid = 1;  // Reparent to init

        // Add to init's children list
        ProcessControlBlock* init = find_process_by_pid(1);
        if (init) {
            ChildNode* new_node = new ChildNode;
            new_node->pid = child_pcb->pid;
            new_node->next = init->children_head;
            init->children_head = new_node;
        }
    }
    child = child->next;
}
```

---

## Files to Create/Modify

### New Files (1)

1. **src/kernel/syscalls/process_mgmt.cpp** (~400 LOC)
   - sys_fork
   - sys_wait4
   - sys_getppid
   - Helper functions

### Files to Modify (5)

1. **src/kernel/pcb.hpp**
   - Add parent_pid
   - Add children_head
   - Add has_exited, has_been_waited

2. **src/kernel/syscalls/basic.cpp**
   - Update sys_exit to wake parent
   - Reparent orphaned children

3. **src/kernel/syscall_table.cpp**
   - Add forward declarations
   - Update table entries

4. **src/kernel/server_spawn.cpp**
   - Initialize parent_pid, children_head

5. **xmake.lua**
   - Add process_mgmt.cpp

---

## Testing Strategy

### Test 1: Basic Fork

```c
pid_t pid = fork();
if (pid == 0) {
    // Child
    write(1, "Child process\n", 14);
    exit(0);
} else {
    // Parent
    write(1, "Parent process\n", 15);
    int status;
    wait4(-1, &status, 0, NULL);
    write(1, "Child exited\n", 13);
}
```

### Test 2: Multiple Children

```c
for (int i = 0; i < 3; i++) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child i
        exit(i);
    }
}

// Parent waits for all children
for (int i = 0; i < 3; i++) {
    int status;
    pid_t child = wait4(-1, &status, 0, NULL);
    printf("Child %d exited with status %d\n", child, status >> 8);
}
```

### Test 3: Parent PID

```c
pid_t ppid = getppid();
printf("My parent is PID %d\n", ppid);
```

---

## Performance Considerations

| Operation | Complexity | Optimization |
|-----------|-----------|--------------|
| fork() | O(n) (stack copy) | Week 10: Copy-on-write |
| wait4() | O(c) (c = children) | Linear search acceptable |
| exit() | O(c + f) (children + FDs) | Batch operations |
| getppid() | O(1) | Direct field access |

---

## Edge Cases & Error Handling

### Fork Failures

1. **No memory:** Return -ENOMEM
2. **Max processes:** Return -EAGAIN
3. **FD table clone fails:** Clean up and return error

### Wait Failures

1. **No children:** Return -ECHILD
2. **Invalid status pointer:** Return -EFAULT
3. **Interrupted:** Return -EINTR (Week 10: signals)

### Orphan Handling

1. **Parent exits first:** Reparent to init (PID 1)
2. **Init must wait:** Init runs reaper loop

---

## Security Considerations

### Week 9 Phase 2

1. **Process isolation:** Each process has own address space (Week 10)
2. **Parent verification:** Only parent can wait for child
3. **Resource limits:** Prevent fork bombs (Week 10)

### Future Enhancements

1. **Fork bomb prevention:** RLIMIT_NPROC
2. **Capabilities:** CAP_SYS_ADMIN for ptrace
3. **Namespaces:** PID namespaces (Week 12)

---

## Expected Boot Sequence

```
[INIT] PID 1 starting
[VFS Server] PID 2 starting (parent=1)
[VFS Server] Testing fork...
[SYSCALL] sys_fork() parent=2, child=5
[VFS Server Child] PID 5 starting (parent=2)
[VFS Server Child] Exiting with status 42
[SYSCALL] sys_exit(42) PID=5
[SYSCALL] sys_wait4(-1) parent=2 reaped child=5, status=42
[VFS Server] Child exited with status 42
```

---

## Implementation Order

1. ✅ **Planning** (this document)
2. **Add parent_pid to PCB**
3. **Implement sys_getppid** (simplest, test infrastructure)
4. **Implement sys_fork** (core complexity)
5. **Implement sys_wait4** (blocking & reaping)
6. **Update sys_exit** (wake parent, reparent)
7. **Add resource cleanup**
8. **Update build system**
9. **Testing**
10. **Documentation**

---

## Success Metrics

- [ ] Fork creates working child process
- [ ] Parent can wait for child
- [ ] Exit status correctly passed
- [ ] Zombies are reaped
- [ ] Orphans reparented to init
- [ ] FD table properly cloned
- [ ] No memory leaks
- [ ] Stack isolation verified

---

## Timeline Estimate

| Task | Time | Complexity |
|------|------|------------|
| PCB modifications | 1 hour | Low |
| sys_getppid | 30 min | Trivial |
| sys_fork | 3-4 hours | High |
| sys_wait4 | 2-3 hours | Medium |
| sys_exit update | 1 hour | Medium |
| Resource cleanup | 1-2 hours | Medium |
| Testing | 2-3 hours | Medium |
| Documentation | 1 hour | Low |
| **Total** | **11-16 hours** | **Phase 2** |

---

## Next Phases

### Week 9 Phase 3: exec() family
- sys_execve - Replace process image
- ELF loader integration
- Argument/environment passing

### Week 10: Memory Management
- Copy-on-write fork
- Demand paging
- mmap/munmap
- Page fault handling

---

## Conclusion

Week 9 Phase 2 establishes **full process lifecycle management** for XINIM. By implementing fork, wait, and proper zombie reaping, we enable multi-process applications, shell job control, and daemon processes.

**After Phase 2, XINIM will support:**
- ✅ Process creation (fork)
- ✅ Parent-child relationships
- ✅ Process waiting and synchronization
- ✅ Proper resource cleanup
- ✅ Orphan handling

This brings XINIM significantly closer to POSIX compliance!

---

**Status:** 📋 **READY TO IMPLEMENT**

**Author:** XINIM Development Team
**Date:** November 18, 2025
**Version:** 1.0

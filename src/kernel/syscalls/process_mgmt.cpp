/**
 * @file process_mgmt.cpp
 * @brief Process management syscalls (fork, wait, getppid)
 *
 * Implements POSIX process lifecycle syscalls for Week 9 Phase 2.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "../syscall_table.hpp"
#include "../pcb.hpp"
#include "../scheduler.hpp"
#include "../uaccess.hpp"
#include "../../early/serial_16550.hpp"
#include <cerrno>
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// Forward declaration of helper functions
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern ProcessControlBlock* allocate_process();
extern void scheduler_add_process(ProcessControlBlock* pcb);
extern ProcessControlBlock* find_process_by_pid(xinim::pid_t pid);

// ============================================================================
// sys_getppid - Get parent process ID
// ============================================================================

/**
 * @brief Get parent process ID
 *
 * POSIX: pid_t getppid(void)
 *
 * @return Parent PID, or 0 if orphaned/init
 */
extern "C" int64_t sys_getppid(uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    return (int64_t)current->parent_pid;
}

// ============================================================================
// sys_fork - Create child process
// ============================================================================

/**
 * @brief Create child process (copy of current process)
 *
 * POSIX: pid_t fork(void)
 *
 * Creates an exact copy of the calling process. The child inherits:
 * - File descriptor table (shallow copy, shared inodes)
 * - Stack (deep copy)
 * - CPU state (except RAX = 0 in child)
 *
 * @return In parent: child PID (positive)
 *         In child: 0
 *         On error: negative error code
 *         -ENOMEM: Not enough memory
 */
extern "C" int64_t sys_fork(uint64_t, uint64_t, uint64_t,
                             uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* parent = get_current_process();
    if (!parent) return -ESRCH;

    // Debug logging
    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_fork() parent PID=%d\n", parent->pid);
    early_serial.write(log_buf);

    // 1. Allocate child PCB
    ProcessControlBlock* child = allocate_process();
    if (!child) {
        early_serial.write("[SYSCALL] sys_fork() failed: ENOMEM (PCB)\n");
        return -ENOMEM;
    }

    // 2. Copy basic process state
    child->name = parent->name;  // Share name string (read-only)
    child->state = ProcessState::READY;
    child->priority = parent->priority;

    // 3. Copy CPU context (all registers)
    child->context = parent->context;

    // 4. CRITICAL: Set child return value to 0
    // When child starts running, it will see fork() return 0
    child->context.rax = 0;

    // 5. Allocate and copy user stack
    child->stack_size = parent->stack_size;
    if (child->stack_size > 0) {
        child->stack_base = kmalloc(child->stack_size);
        if (!child->stack_base) {
            early_serial.write("[SYSCALL] sys_fork() failed: ENOMEM (user stack)\n");
            kfree(child);
            return -ENOMEM;
        }
        std::memcpy(child->stack_base, parent->stack_base, child->stack_size);
    } else {
        child->stack_base = nullptr;
    }

    // 6. Allocate and copy kernel stack
    child->kernel_stack_size = parent->kernel_stack_size;
    if (child->kernel_stack_size > 0) {
        child->kernel_stack_base = kmalloc(child->kernel_stack_size);
        if (!child->kernel_stack_base) {
            early_serial.write("[SYSCALL] sys_fork() failed: ENOMEM (kernel stack)\n");
            if (child->stack_base) kfree(child->stack_base);
            kfree(child);
            return -ENOMEM;
        }
        std::memcpy(child->kernel_stack_base, parent->kernel_stack_base,
                   child->kernel_stack_size);

        // Copy kernel RSP
        child->kernel_rsp = parent->kernel_rsp;
    } else {
        child->kernel_stack_base = nullptr;
        child->kernel_rsp = 0;
    }

    // 7. Clone file descriptor table
    int ret = parent->fd_table.clone_to(&child->fd_table);
    if (ret < 0) {
        early_serial.write("[SYSCALL] sys_fork() failed: FD table clone\n");
        if (child->kernel_stack_base) kfree(child->kernel_stack_base);
        if (child->stack_base) kfree(child->stack_base);
        kfree(child);
        return ret;
    }

    // 8. Set up parent-child relationship
    child->parent_pid = parent->pid;
    child->children_head = nullptr;  // Child has no children yet
    child->has_exited = false;
    child->has_been_waited = false;
    child->exit_status = 0;

    // 9. Add child to parent's children list
    ChildNode* child_node = new ChildNode;
    if (!child_node) {
        early_serial.write("[SYSCALL] sys_fork() failed: ENOMEM (child node)\n");
        if (child->kernel_stack_base) kfree(child->kernel_stack_base);
        if (child->stack_base) kfree(child->stack_base);
        kfree(child);
        return -ENOMEM;
    }
    child_node->pid = child->pid;
    child_node->next = parent->children_head;
    parent->children_head = child_node;

    // 10. Copy time accounting
    child->time_quantum_start = 0;
    child->total_ticks = 0;

    // 11. Initialize blocking state
    child->blocked_on = BlockReason::NONE;
    child->ipc_wait_source = 0;

    // 12. Add child to scheduler (makes it READY to run)
    scheduler_add_process(child);

    // Debug logging
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_fork() parent=%d, child=%d\n",
                  parent->pid, child->pid);
    early_serial.write(log_buf);

    // 13. Return child PID to parent
    return (int64_t)child->pid;
}

// ============================================================================
// sys_wait4 - Wait for child process to exit
// ============================================================================

/**
 * @brief Wait for child process to change state
 *
 * POSIX: pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage)
 *
 * Week 9 Phase 2 limitations:
 * - Only supports pid=-1 (any child)
 * - No WNOHANG support (always blocks)
 * - No rusage support
 *
 * @param pid_arg Child to wait for (-1 = any child)
 * @param status_addr User space pointer to store exit status (can be 0)
 * @param options Wait options (currently ignored)
 * @param rusage_addr Resource usage (currently ignored)
 * @return Child PID on success, negative error on failure
 *         -ECHILD: No children exist
 *         -EFAULT: Invalid status pointer
 */
extern "C" int64_t sys_wait4(uint64_t pid_arg, uint64_t status_addr,
                              uint64_t options, uint64_t rusage_addr,
                              uint64_t, uint64_t) {
    ProcessControlBlock* parent = get_current_process();
    if (!parent) return -ESRCH;

    int target_pid = (int)pid_arg;

    // Week 9: Only support pid = -1 (any child)
    if (target_pid != -1) {
        return -ENOSYS;  // Specific PID wait not yet implemented
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

        if (child && child->state == ProcessState::ZOMBIE && !child->has_been_waited) {
            // Found zombie child that hasn't been waited on!

            // Copy exit status to user space
            if (status_addr != 0) {
                // POSIX format: exit_status << 8
                int status = child->exit_status << 8;
                int ret = copy_to_user(status_addr, &status, sizeof(int));
                if (ret < 0) {
                    return ret;  // Invalid status pointer
                }
            }

            xinim::pid_t child_pid = child->pid;

            // Mark as waited
            child->has_been_waited = true;

            // Debug logging
            char log_buf[128];
            std::snprintf(log_buf, sizeof(log_buf),
                          "[SYSCALL] sys_wait4() parent=%d reaped child=%d, status=%d\n",
                          parent->pid, child_pid, child->exit_status);
            early_serial.write(log_buf);

            // Remove from children list
            if (prev) {
                prev->next = curr->next;
            } else {
                parent->children_head = curr->next;
            }
            delete curr;

            // Free child resources (Week 9 Phase 2: Done here)
            // Week 10: Will be done in separate reaper
            if (child->stack_base) {
                kfree(child->stack_base);
                child->stack_base = nullptr;
            }
            if (child->kernel_stack_base) {
                kfree(child->kernel_stack_base);
                child->kernel_stack_base = nullptr;
            }

            // Close all FDs
            for (size_t i = 0; i < MAX_FDS_PER_PROCESS; i++) {
                if (child->fd_table.fds[i].is_open) {
                    child->fd_table.close_fd((int)i);
                }
            }

            // Mark as DEAD (scheduler will skip)
            child->state = ProcessState::DEAD;

            // Note: PCB itself is freed by scheduler eventually
            // Week 10: Proper reaper thread

            return (int64_t)child_pid;
        }

        prev = curr;
        curr = curr->next;
    }

    // No zombie children found - block parent
    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_wait4() parent=%d blocking (no zombies yet)\n",
                  parent->pid);
    early_serial.write(log_buf);

    parent->state = ProcessState::BLOCKED;
    parent->blocked_on = BlockReason::WAIT_CHILD;

    // Yield to scheduler (will skip this process until unblocked)
    schedule();

    // When we return here, a child should have exited and woken us
    // Retry the wait
    return sys_wait4(pid_arg, status_addr, options, rusage_addr, 0, 0);
}

} // namespace xinim::kernel

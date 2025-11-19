/**
 * @file pcb.hpp
 * @brief Process Control Block - Authoritative Definition
 *
 * This header defines the Process Control Block (PCB) structure used
 * throughout the XINIM kernel. This is the SINGLE authoritative definition
 * to prevent conflicts between server_spawn.cpp and scheduler.cpp.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_PCB_HPP
#define XINIM_KERNEL_PCB_HPP

#include <cstdint>
#include "../include/sys/type.hpp"  // For pid_t
#include "context.hpp"               // For CpuContext
#include "fd_table.hpp"              // For FileDescriptorTable (Week 9 Phase 1)

// Forward declaration for signal state (Week 10 Phase 2)
namespace xinim::kernel {
    struct SignalState;
}

namespace xinim::kernel {

/**
 * @brief Process state enumeration
 *
 * Represents the current execution state of a process.
 */
enum class ProcessState {
    CREATED,    ///< PCB allocated, not yet ready to run
    READY,      ///< Ready to be scheduled
    RUNNING,    ///< Currently executing
    BLOCKED,    ///< Waiting for IPC, I/O, or event
    ZOMBIE,     ///< Exited but not yet reaped
    DEAD        ///< Fully cleaned up
};

/**
 * @brief Block reason for BLOCKED processes
 *
 * When a process is in the BLOCKED state, this indicates why.
 */
enum class BlockReason {
    NONE,           ///< Not blocked
    IPC_RECV,       ///< Waiting for IPC message
    IPC_SEND,       ///< Waiting for IPC send completion
    TIMER,          ///< Sleeping (waiting for timeout)
    IO,             ///< Waiting for I/O completion
    WAIT_CHILD      ///< Waiting for child process (Week 9 Phase 2)
};

/**
 * @brief Child process node for parent's children list
 *
 * Week 9 Phase 2: Tracks parent-child relationships.
 * Parent maintains linked list of all child PIDs.
 */
struct ChildNode {
    xinim::pid_t pid;       ///< Child process PID
    ChildNode* next;        ///< Next child in list
};

/**
 * @brief Process Control Block
 *
 * Stores complete process state including:
 * - Process identity (PID, name)
 * - Execution state (READY, RUNNING, BLOCKED, etc.)
 * - CPU context (registers, stack pointer, instruction pointer)
 * - Memory allocation (stack base/size)
 * - Scheduling metadata (priority, time quantum)
 * - Blocking information (why blocked, what waiting for)
 * - Queue linkage (for scheduler queues)
 *
 * Size: ~280 bytes (depends on CpuContext size)
 */
struct ProcessControlBlock {
    // ========================================
    // Process Identity
    // ========================================

    xinim::pid_t pid;               ///< Process ID
    const char* name;               ///< Human-readable name (for debugging)

    // ========================================
    // Execution State
    // ========================================

    ProcessState state;             ///< Current state (READY, RUNNING, etc.)
    uint32_t priority;              ///< Scheduling priority (0-31, higher = more important)

    // ========================================
    // Memory Allocation
    // ========================================

    void* stack_base;               ///< Base of user stack allocation
    uint64_t stack_size;            ///< User stack size in bytes

    // Week 8 Phase 3: Kernel stack for Ring 3 processes
    void* kernel_stack_base;        ///< Base of kernel stack allocation
    uint64_t kernel_stack_size;     ///< Kernel stack size in bytes (typically 4 KB)
    uint64_t kernel_rsp;            ///< Current kernel stack pointer (top of stack)

    // ========================================
    // CPU Context
    // ========================================

    CpuContext context;             ///< Saved CPU context (for context switch)

    // ========================================
    // Blocking Information
    // ========================================

    BlockReason blocked_on;         ///< Why this process is blocked
    xinim::pid_t ipc_wait_source;   ///< PID we're waiting for (if IPC_RECV), or 0

    // ========================================
    // Time Accounting
    // ========================================

    uint64_t time_quantum_start;    ///< Tick when this quantum started
    uint64_t total_ticks;           ///< Total CPU ticks consumed by this process

    // ========================================
    // File Descriptor Table (Week 9 Phase 1)
    // ========================================

    FileDescriptorTable fd_table;   ///< Per-process file descriptor table

    // ========================================
    // Process Exit Status
    // ========================================

    int exit_status;                ///< Exit status code (for wait/waitpid)

    // ========================================
    // Parent-Child Relationships (Week 9 Phase 2)
    // ========================================

    xinim::pid_t parent_pid;        ///< Parent process PID (0 if orphaned or init)
    ChildNode* children_head;       ///< Head of children list (linked list)

    bool has_exited;                ///< Has this process called exit()?
    bool has_been_waited;           ///< Has parent called wait() on this zombie?

    // ========================================
    // Memory Management (Week 10 Phase 1)
    // ========================================

    uint64_t brk;                   ///< Current program break (end of heap)

    // ========================================
    // Signal Handling (Week 10 Phase 2)
    // ========================================

    SignalState* signal_state;      ///< Signal state (handlers, pending, blocked)

    // ========================================
    // Process Groups and Sessions (Week 10 Phase 3)
    // ========================================

    xinim::pid_t pgid;              ///< Process group ID
    xinim::pid_t sid;               ///< Session ID
    ProcessControlBlock* pg_next;   ///< Next process in same process group
    ProcessControlBlock* pg_prev;   ///< Previous process in same process group

    // ========================================
    // Scheduler Queue Linkage
    // ========================================

    ProcessControlBlock* next;      ///< Next PCB in queue (for FIFO/priority queues)
    ProcessControlBlock* prev;      ///< Previous PCB in queue (for doubly-linked lists)
};

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_PCB_HPP */

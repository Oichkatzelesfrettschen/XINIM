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
 */

#ifndef XINIM_KERNEL_PCB_HPP
#define XINIM_KERNEL_PCB_HPP

#include "../include/sys/type.hpp" // For pid_t
#include "context.hpp"             // For CpuContext
#include "fd_table.hpp"            // For FileDescriptorTable
#include "user_mapping_table.hpp"

#include <array>
#include <cstdint>

// Forward declaration for per-process signal state.
namespace xinim::kernel {

    /**
     * @brief Maximum length stored for a process name (including terminator).
     */
    inline constexpr std::size_t kProcessNameMax = 128;
    struct SignalState;
} // namespace xinim::kernel

namespace xinim::kernel {

    /**
     * @brief Process state enumeration
     *
     * Represents the current execution state of a process.
     */
    enum class ProcessState {
        CREATED, ///< PCB allocated, not yet ready to run
        READY,   ///< Ready to be scheduled
        RUNNING, ///< Currently executing
        BLOCKED, ///< Waiting for IPC, I/O, or event
        STOPPED, ///< Stopped by signal (job control)
        ZOMBIE,  ///< Exited but not yet reaped
        DEAD     ///< Fully cleaned up
    };

    /**
     * @brief Block reason for BLOCKED processes
     *
     * When a process is in the BLOCKED state, this indicates why.
     */
    enum class BlockReason {
        NONE,       ///< Not blocked
        IPC_RECV,   ///< Waiting for IPC message
        IPC_SEND,   ///< Waiting for IPC send completion
        TIMER,      ///< Sleeping (waiting for timeout)
        IO,         ///< Waiting for I/O completion
        SELECT,     ///< Waiting for descriptor readiness or a deadline
        WAIT_CHILD, ///< Waiting for a child process state change
        WAIT_SIGNAL ///< Waiting for an unblocked signal
    };

    /**
     * @brief Child process node for parent's children list
     *
     * The parent maintains a linked list of all child PIDs.
     */
    struct ChildNode {
        xinim::pid_t pid; ///< Child process PID
        ChildNode *next;  ///< Next child in list
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

        xinim::pid_t pid;                                 ///< Process ID
        std::array<char, kProcessNameMax> name_storage{}; ///< Backing storage for name.
        const char *name; ///< Human-readable name (points into name_storage or static data)

        // ========================================
        // Execution State
        // ========================================

        ProcessState state;        ///< Current state (READY, RUNNING, etc.)
        uint32_t priority;         ///< Current effective scheduling priority
        uint32_t base_priority;    ///< Baseline priority before runtime penalties/boosts
        uint32_t quantum_ticks;    ///< Explicit quantum override (0 = policy default)
        uint16_t scheduler_domain; ///< Scheduling domain / partition identifier

        // ========================================
        // Memory Allocation
        // ========================================

        void *stack_base;    ///< Base of user stack allocation
        uint64_t stack_size; ///< User stack size in bytes

        // Kernel stack and address-space root for Ring 3 processes.
        void *kernel_stack_base;     ///< Base of kernel stack allocation
        uint64_t kernel_stack_size;  ///< Kernel stack size in bytes (typically 4 KB)
        uint64_t kernel_rsp;         ///< Current kernel stack pointer (top of stack)
        uint64_t address_space_root; ///< Physical CR3 root owned by this process

        // ========================================
        // CPU Context
        // ========================================

        CpuContext context; ///< Saved CPU context (for context switch)

        // ========================================
        // Blocking Information
        // ========================================

        BlockReason blocked_on;       ///< Why this process is blocked
        xinim::pid_t ipc_wait_source; ///< PID we're waiting for (if IPC_RECV), or 0

        // ========================================
        // Time Accounting
        // ========================================

        uint64_t time_quantum_start;       ///< Tick when this quantum started
        uint64_t total_ticks;              ///< Total CPU ticks consumed by this process
        uint64_t children_ticks;           ///< CPU ticks consumed by reaped descendants.
        uint64_t wake_deadline_tick;       ///< Scheduler tick that completes a sleep.
        uintptr_t sleep_remaining_address; ///< Optional nanosleep remainder output.
        uint64_t select_deadline_tick;     ///< Absolute descriptor-wait deadline.
        uintptr_t select_timeout_address;  ///< Optional select timeval output.
        bool select_active;                ///< A restarted select retains its deadline.
        uint64_t alarm_deadline_tick;      ///< Scheduler tick that raises SIGALRM.

        // ========================================
        // File Descriptor Table
        // ========================================

        FileDescriptorTable fd_table;              ///< Per-process file descriptor table
        std::array<char, 256> current_directory{}; ///< Absolute working directory.

        // ========================================
        // Process Exit Status
        // ========================================

        int exit_status;        ///< Exit status code (for wait/waitpid)
        int termination_signal; ///< Fatal signal, or zero for normal exit.
        int stop_signal;        ///< Signal responsible for the stop event.

        // ========================================
        // Parent-Child Relationships
        // ========================================

        xinim::pid_t parent_pid;  ///< Parent process PID (0 if orphaned or init)
        ChildNode *children_head; ///< Head of children list (linked list)

        bool has_exited;               ///< Has this process called exit()?
        bool has_been_waited;          ///< Has parent called wait() on this zombie?
        bool has_execed;               ///< Child completed an exec boundary.
        bool wait_stopped_pending;     ///< Parent has not consumed the stop event.
        bool wait_continued_pending;   ///< Parent has not consumed the continue event.
        uintptr_t wait_status_address; ///< Parent user address for wait status
        xinim::pid_t wait_target_pid;  ///< Child selected by a blocking wait
        int wait_options;              ///< Options retained by a blocking wait.

        // ========================================
        // Memory Management
        // ========================================

        uint64_t brk;                   ///< Current program break (end of heap)
        uint64_t minimum_break;         ///< Lowest valid program break for this image.
        UserMappingTable user_mappings; ///< Anonymous mappings owned by the process.
        uint64_t descriptor_limit;      ///< Soft RLIMIT_NOFILE value.
        uint32_t file_creation_mask;    ///< Mode bits removed during file creation.
        int nice_value;                 ///< POSIX nice value in the range -20 to 19.
        uint32_t real_user_id;
        uint32_t effective_user_id;
        uint32_t saved_user_id;
        uint32_t real_group_id;
        uint32_t effective_group_id;
        uint32_t saved_group_id;
        std::array<uint32_t, 32U> supplementary_groups{};
        size_t supplementary_group_count;

        // ========================================
        // Signal Handling
        // ========================================

        SignalState *signal_state; ///< Signal state (handlers, pending, blocked)

        // ========================================
        // Process Groups and Sessions
        // ========================================

        xinim::pid_t pgid;            ///< Process group ID
        xinim::pid_t sid;             ///< Session ID
        ProcessControlBlock *pg_next; ///< Next process in same process group
        ProcessControlBlock *pg_prev; ///< Previous process in same process group

        // ========================================
        // Scheduler Queue Linkage
        // ========================================

        ProcessControlBlock *next; ///< Next PCB in queue (for FIFO/priority queues)
        ProcessControlBlock *prev; ///< Previous PCB in queue (for doubly-linked lists)
    };

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_PCB_HPP */

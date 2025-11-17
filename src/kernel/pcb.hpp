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
    IO              ///< Waiting for I/O completion
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

    void* stack_base;               ///< Base of stack allocation
    uint64_t stack_size;            ///< Stack size in bytes

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
    // Scheduler Queue Linkage
    // ========================================

    ProcessControlBlock* next;      ///< Next PCB in queue (for FIFO/priority queues)
    ProcessControlBlock* prev;      ///< Previous PCB in queue (for doubly-linked lists)
};

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_PCB_HPP */

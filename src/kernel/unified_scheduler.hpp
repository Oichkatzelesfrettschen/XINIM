#pragma once
/**
 * @file unified_scheduler.hpp
 * @brief O(1) bitmap-based multi-level feedback queue scheduler.
 *
 * WHY: The kernel had two incompatible schedulers -- proc.cpp (MINIX heritage
 *      priority linked-list) and schedule.cpp (flat FIFO with wait-for graph).
 *      Neither alone was sufficient. This synthesis provides:
 *      - O(1) pick_next via bitmap + per-priority run queues (from proc.cpp)
 *      - Wait-for-graph deadlock detection (from schedule.cpp)
 *      - PCB-based process tracking
 *      - Configurable per-priority time quanta
 *
 * WHAT: Single UnifiedScheduler class that all callers use.
 *
 * HOW: uint64_t bitmap (1 bit per priority), doubly-linked PCB run queues,
 *      __builtin_ctzll for O(1) highest-priority queue lookup.
 */

#include "../include/sys/type.hpp"
#include "pcb.hpp"
#include "scheduler_policy.hpp"
#include "wait_graph.hpp"

#include <cstdint>

namespace xinim::kernel {

    /**
     * @brief Number of priority levels (fits in one 64-bit bitmap).
     */
    inline constexpr int NUM_PRIORITIES = sched_policy::NUM_PRIORITIES;

    /**
     * @brief Priority level constants.
     */
    inline constexpr uint32_t PRIO_INTERRUPT = sched_policy::PRIO_INTERRUPT;
    inline constexpr uint32_t PRIO_SYSTEM_LO = sched_policy::PRIO_SYSTEM_LO;
    inline constexpr uint32_t PRIO_SYSTEM_HI = sched_policy::PRIO_SYSTEM_HI;
    inline constexpr uint32_t PRIO_SERVER_LO = sched_policy::PRIO_SERVER_LO;
    inline constexpr uint32_t PRIO_SERVER_HI = sched_policy::PRIO_SERVER_HI;
    inline constexpr uint32_t PRIO_USER_HIGH = sched_policy::PRIO_USER_HIGH;
    inline constexpr uint32_t PRIO_USER_NORM = sched_policy::PRIO_USER_NORM;
    inline constexpr uint32_t PRIO_USER_LOW = sched_policy::PRIO_USER_LOW;
    inline constexpr uint32_t PRIO_IDLE = sched_policy::PRIO_IDLE;

    /**
     * @brief Maximum number of processes tracked by the scheduler.
     */
    inline constexpr int MAX_PROCESSES = sched_policy::MAX_PROCESSES;

    /**
     * @brief Per-priority time quantum (in timer ticks).
     *
     * System tasks (0-3) get unlimited quanta.
     * Lower priority = shorter slice.
     */
    inline constexpr uint32_t quantum_for_priority(uint32_t priority) {
        return sched_policy::quantum_for_priority(priority);
    }

    inline constexpr uint64_t PRIORITY_REBALANCE_PERIOD_TICKS =
        sched_policy::PRIORITY_REBALANCE_PERIOD_TICKS;

    /**
     * @brief O(1) bitmap-based unified scheduler.
     */
    class UnifiedScheduler {
    public:
        UnifiedScheduler();

        // -- Process registration --

        /**
         * @brief Register a PCB with the scheduler.
         *
         * The PCB is added to the process table and enqueued if READY.
         */
        void add_process(ProcessControlBlock *pcb);

        /**
         * @brief Detach a non-running process from all scheduler-owned structures.
         *
         * Resource destruction remains the caller's responsibility and must occur
         * from a different process's kernel stack.
         */
        bool remove_process(ProcessControlBlock *pcb);

        // -- Run queue operations (all O(1)) --

        /**
         * @brief Add a READY process to its priority run queue.
         */
        void enqueue(ProcessControlBlock *pcb);

        /**
         * @brief Remove a process from its priority run queue.
         */
        void dequeue(ProcessControlBlock *pcb);

        /**
         * @brief Select the highest-priority runnable process. O(1).
         *
         * @return PCB of next process, or nullptr if no runnable process.
         */
        ProcessControlBlock *pick_next();

        // -- Blocking / unblocking --

        /**
         * @brief Block a process.
         *
         * Removes from run queue, sets state to BLOCKED, records reason.
         * If target != -1, adds a wait-for edge for deadlock detection.
         *
         * @return false if blocking would create a deadlock cycle.
         */
        bool block(ProcessControlBlock *pcb, BlockReason reason, xinim::pid_t target = -1);

        /**
         * @brief Unblock a process and add it back to the run queue.
         */
        void unblock(xinim::pid_t pid);

        /** Remove a process from execution until a continue signal arrives. */
        void stop(ProcessControlBlock *pcb);

        // -- Yield --

        /**
         * @brief Current process yields the CPU.
         *
         * Re-enqueues current at tail of its priority queue.
         */
        void yield();

        /**
         * @brief Yield to a specific process (e.g., IPC fast-path).
         */
        void yield_to(xinim::pid_t pid);

        // -- Quantum management --

        /**
         * @brief Called from timer interrupt. Decrements quantum, yields on expiry.
         */
        void timer_tick(bool allow_preemption = true);

        // -- Accessors --

        ProcessControlBlock *current() const { return current_; }
        xinim::pid_t current_pid() const { return current_ ? current_->pid : -1; }
        ProcessControlBlock *find_by_pid(xinim::pid_t pid) const;

        uint64_t tick_count() const { return tick_count_; }

        const lattice::WaitForGraph &wait_graph() const { return wait_graph_; }

    private:
        uint32_t effective_quantum(const ProcessControlBlock *pcb) const;
        void rebalance_priorities();

        // Per-priority doubly-linked list heads and tails
        ProcessControlBlock *run_queue_head_[NUM_PRIORITIES]{};
        ProcessControlBlock *run_queue_tail_[NUM_PRIORITIES]{};

        // Bitmap: bit i set iff run_queue_head_[i] != nullptr
        uint64_t priority_bitmap_{0};

        // Currently running process
        ProcessControlBlock *current_{nullptr};

        // Process table (indexed by PID for O(1) lookup)
        ProcessControlBlock *proc_table_[MAX_PROCESSES]{};

        // Deadlock detection
        lattice::WaitForGraph wait_graph_{};

        // Global tick counter
        uint64_t tick_count_{0};

        // Ticks remaining for current process
        uint32_t ticks_remaining_{0};
    };

    /**
     * @brief Global unified scheduler instance.
     */
    extern UnifiedScheduler g_unified_scheduler;

} // namespace xinim::kernel

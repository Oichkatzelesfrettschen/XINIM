#pragma once
#include "../include/xinim/core_types.hpp"
#include "wait_graph.hpp"
#include <array>
#include <optional>

namespace sched {

/**
 * @brief Preemptive round-robin scheduler for XINIM.
 * Refactored for bare-metal: uses fixed-size arrays instead of std containers.
 */
class Scheduler {
  public:
    static constexpr int MAX_PROCS = 64; // NR_TASKS + NR_PROCS + padding

    Scheduler() : ready_head_(0), ready_tail_(0), current_(-1) {
        for (int i = 0; i < MAX_PROCS; ++i) waiting_[i] = -1;
    }

    /** @brief Select the next process to run. */
    xinim::pid_t pick_next() noexcept;

    /** @brief Add a process to the ready queue. */
    void ready(xinim::pid_t pid) noexcept;

    /** @brief Remove a process from the ready queue. */
    void unready(xinim::pid_t pid) noexcept;

    /** @brief Block process @p pid waiting for @p target. */
    bool block_on(xinim::pid_t pid, xinim::pid_t target) noexcept;

    /** @brief Unblock process @p pid. */
    void unblock(xinim::pid_t pid) noexcept;

    /** @brief Relinquish CPU. */
    void yield() noexcept;

    /** @brief Yield CPU specifically to process @p pid. */
    void yield_to(xinim::pid_t pid) noexcept;

    /** @brief Handle service crash. */
    void handle_crash(xinim::pid_t pid);

    [[nodiscard]] xinim::pid_t current() const noexcept { return current_; }
    [[nodiscard]] const lattice::WaitForGraph& graph() const noexcept { return graph_; }

  private:
    xinim::pid_t ready_queue_[MAX_PROCS]{};
    int ready_head_;
    int ready_tail_;
    
    xinim::pid_t current_;
    xinim::pid_t waiting_[MAX_PROCS]; ///< blocking edges: pid -> target_pid
    lattice::WaitForGraph graph_{}; ///< wait-for graph for deadlock detection
};

extern Scheduler scheduler;

} // namespace sched

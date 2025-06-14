#pragma once
#include "../include/xinim/core_types.hpp"
#include <algorithm>
#include <deque>
#include <optional>

namespace sched {

/**
 * @brief Cooperative scheduler with a simple FIFO run queue.
 */
class Scheduler {
  public:
    /// Add a thread to the ready queue.
    void enqueue(xinim::pid_t pid) { ready_.push_back(pid); }

    /**
     * @brief Preempt the current thread and schedule the next one.
     *
     * The current thread is placed at the end of the queue if valid.
     * @return Identifier of the thread now running or std::nullopt if none.
     */
    std::optional<xinim::pid_t> preempt();

    /**
     * @brief Yield execution directly to a specific thread.
     *
     * The current thread is enqueued and @p target becomes current if present
     * in the ready queue.
     * @param target Thread to switch to.
     */
    void yield_to(xinim::pid_t target);

    /// Currently running thread identifier.
    [[nodiscard]] xinim::pid_t current() const noexcept { return current_; }

  private:
    std::deque<xinim::pid_t> ready_{}; ///< ready queue
    xinim::pid_t current_{-1};         ///< id of running thread
};

/// Global scheduler instance used by tests.
extern Scheduler scheduler;

} // namespace sched

#pragma once
#include "../include/xinim/core_types.hpp"
#include "wait_graph.hpp"
#include <algorithm>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>

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

    /**
     * @brief Notify the scheduler that a service crashed.
     *
     * The scheduler delegates restart handling and liveness checks to the
     * global service manager.
     *
     * @param pid Identifier of the crashed service.
     */
    void crash(xinim::pid_t pid);

    /// Currently running thread identifier.
    [[nodiscard]] xinim::pid_t current() const noexcept { return current_; }

    /**
     * @brief Block @p src until @p dst unblocks.
     *
     * The method records the dependency in the wait-for graph and fails if
     * doing so would create a cycle.
     *
     * @param src Blocking thread identifier.
     * @param dst Thread being waited on.
     * @return @c true if blocking succeeded.
     */
    [[nodiscard]] bool block_on(xinim::pid_t src, xinim::pid_t dst);

    /**
     * @brief Unblock the given thread.
     *
     * Any wait-for edges originating from @p pid are removed and the thread
     * becomes runnable again.
     *
     * @param pid Identifier to unblock.
     */
    void unblock(xinim::pid_t pid);

    /**
     * @brief Check whether a thread is currently blocked.
     *
     * @param pid Identifier to query.
     * @return @c true if the thread is blocked.
     */
    [[nodiscard]] bool is_blocked(xinim::pid_t pid) const noexcept;

    /**
     * @brief Determine the next runnable thread without altering state.
     *
     * Returns the identifier at the front of the ready queue or @c -1 when
     * no runnable threads exist.
     */
    [[nodiscard]] xinim::pid_t pick() const noexcept;

    /**
     * @brief Yield directly to @p receiver when available.
     *
     * The current thread is queued and @p receiver becomes current if found in
     * the ready queue.  This mirrors a direct hand-off in message passing
     * implementations.
     *
     * @param receiver Identifier of the thread to run next.
     */
    void direct_handoff(xinim::pid_t receiver);

    /**
     * @brief Access the internal wait-for graph for inspection.
     */
    [[nodiscard]] const lattice::WaitForGraph &graph() const noexcept { return graph_; }

  private:
    std::deque<xinim::pid_t> ready_{};             ///< ready queue
    xinim::pid_t current_{-1};                     ///< id of running thread
    std::unordered_set<xinim::pid_t> blocked_{};   ///< set of blocked threads
    std::unordered_map<xinim::pid_t, xinim::pid_t> ///< blocking edges
        waiting_{};
    lattice::WaitForGraph graph_{}; ///< wait-for graph for deadlock detection
};

/// Global scheduler instance used by tests.
extern Scheduler scheduler;

} // namespace sched

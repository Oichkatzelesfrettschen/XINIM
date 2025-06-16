#include "schedule.hpp"
#include "service.hpp"

namespace sched {

/// Global scheduler instance used by kernel tests.
Scheduler scheduler{};

/**
 * @brief Select the next runnable thread.
 *
 * Requeues the current thread when it remains runnable and switches to the
 * front of the ready queue.
 *
 * @return Identifier of the thread now running or @c std::nullopt when the
 *         queue is empty.
 */
std::optional<xinim::pid_t> Scheduler::preempt() {
    if (ready_.empty()) {
        current_ = -1;
        return std::nullopt;
    }

    if (current_ != -1 && !blocked_.contains(current_)) {
        ready_.push_back(current_);
    }

    current_ = ready_.front();
    ready_.pop_front();
    return current_;
}

/**
 * @brief Yield execution to @p target when it is runnable.
 *
 * The current thread is queued and control transfers to the selected thread if
 * it is present in the ready list.
 *
 * @param target Identifier of the thread to run next.
 */
void Scheduler::yield_to(xinim::pid_t target) {
    if (!std::erase(ready_, target)) {
        return; // target not runnable
    }
    if (current_ != -1) {
        ready_.push_back(current_);
    }
    current_ = target;
}

/**
 * @brief Block @p src until @p dst is runnable.
 *
 * The wait-for graph is updated and @p src removed from the ready queue. The
 * operation fails if adding the edge creates a cycle.
 *
 * @param src Identifier of the blocking thread.
 * @param dst Thread being awaited.
 * @return @c true on success.
 */
bool Scheduler::block_on(xinim::pid_t src, xinim::pid_t dst) {
    if (graph_.add_edge(src, dst)) {
        return false;
    }

    waiting_[src] = dst;
    blocked_.insert(src);

    std::erase(ready_, src);

    if (current_ == src) {
        preempt();
    }
    return true;
}

/**
 * @brief Unblock @p pid and return it to the ready queue.
 *
 * Any wait-for edges from the thread are removed before it is requeued.
 *
 * @param pid Thread identifier to unblock.
 */
void Scheduler::unblock(xinim::pid_t pid) {
    if (auto it = waiting_.find(pid); it != waiting_.end()) {
        graph_.remove_edge(pid, it->second);
        waiting_.erase(it);
    }

    if (blocked_.erase(pid)) {
        ready_.push_back(pid);
    }
}

/**
 * @brief Check if a thread is blocked.
 *
 * @param pid Thread identifier to query.
 * @return @c true when the thread is blocked.
 */
bool Scheduler::is_blocked(xinim::pid_t pid) const noexcept { return blocked_.contains(pid); }

/**
 * @brief Notify the service manager that a service crashed.
 *
 * @param pid Identifier of the failing service.
 */
void Scheduler::crash(xinim::pid_t pid) {
    if (!svc::service_manager.handle_crash(pid) && current_ == pid) {
        // Service exceeded restart limit; drop the thread from scheduling.
        current_ = -1;
    }
}

} // namespace sched

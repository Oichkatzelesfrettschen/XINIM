#include "schedule.hpp"
#include "service.hpp"

namespace sched {

Scheduler scheduler{}; ///< Global instance used by kernel tests

/// Preempt the current thread and schedule the next ready thread.
///
/// @return PID of the thread now running or std::nullopt when none are ready.
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

/// Yield execution directly to a specific thread if it is runnable.
///
/// @param target Thread identifier to switch to.
/// @return void
void Scheduler::yield_to(xinim::pid_t target) {
    if (!std::erase(ready_, target)) {
        return; // target not runnable
    }
    if (current_ != -1) {
        ready_.push_back(current_);
    }
    current_ = target;
}

/// Block @p src until @p dst is unblocked.
///
/// @param src Blocking thread identifier.
/// @param dst Thread being waited on.
/// @return True if blocking succeeds without creating a cycle.
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

/// Unblock the given thread and make it runnable again.
///
/// @param pid Identifier to unblock.
/// @return void
void Scheduler::unblock(xinim::pid_t pid) {
    if (auto it = waiting_.find(pid); it != waiting_.end()) {
        graph_.remove_edge(pid, it->second);
        waiting_.erase(it);
    }

    if (blocked_.erase(pid)) {
        ready_.push_back(pid);
    }
}

/// Query whether a thread is currently blocked.
///
/// @param pid Identifier to test.
/// @return True if the thread is blocked.
bool Scheduler::is_blocked(xinim::pid_t pid) const noexcept { return blocked_.contains(pid); }

/// Delegate crash handling for @p pid to the service manager.
///
/// @param pid Identifier of the crashed service.
/// @return void
void Scheduler::crash(xinim::pid_t pid) { svc::service_manager.handle_crash(pid); }

} // namespace sched

#include "schedule.hpp"

namespace sched {

Scheduler scheduler{}; ///< Global instance used by kernel tests

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

void Scheduler::yield_to(xinim::pid_t target) {
    auto it = std::find(ready_.begin(), ready_.end(), target);
    if (it == ready_.end()) {
        return; // target not runnable
    }
    if (current_ != -1) {
        ready_.push_back(current_);
    }
    ready_.erase(it);
    current_ = target;
}

bool Scheduler::block_on(xinim::pid_t src, xinim::pid_t dst) {
    if (graph_.add_edge(src, dst)) {
        return false;
    }

    waiting_[src] = dst;
    blocked_.insert(src);

    auto it = std::find(ready_.begin(), ready_.end(), src);
    if (it != ready_.end()) {
        ready_.erase(it);
    }

    if (current_ == src) {
        preempt();
    }
    return true;
}

void Scheduler::unblock(xinim::pid_t pid) {
    auto it = waiting_.find(pid);
    if (it != waiting_.end()) {
        graph_.remove_edge(pid, it->second);
        waiting_.erase(it);
    }

    if (blocked_.erase(pid) > 0) {
        ready_.push_back(pid);
    }
}

bool Scheduler::is_blocked(xinim::pid_t pid) const noexcept { return blocked_.contains(pid); }

} // namespace sched

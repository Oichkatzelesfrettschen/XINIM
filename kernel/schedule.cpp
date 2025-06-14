#include "schedule.hpp"

namespace sched {

Scheduler scheduler{}; ///< Global instance used by kernel tests

std::optional<xinim::pid_t> Scheduler::preempt() {
    if (ready_.empty()) {
        return std::nullopt;
    }
    if (current_ != -1) {
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

} // namespace sched

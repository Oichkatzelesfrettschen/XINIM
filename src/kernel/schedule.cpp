#include "schedule.hpp"
#include "service.hpp"

namespace sched {

Scheduler scheduler{};

xinim::pid_t Scheduler::pick_next() noexcept {
    if (ready_head_ == ready_tail_) {
        current_ = -1;
        return -1;
    }
    
    current_ = ready_queue_[ready_head_];
    ready_head_ = (ready_head_ + 1) % MAX_PROCS;
    return current_;
}

void Scheduler::ready(xinim::pid_t pid) noexcept {
    // Check if already in queue (inefficient but safe for small MAX_PROCS)
    for (int i = ready_head_; i != ready_tail_; i = (i + 1) % MAX_PROCS) {
        if (ready_queue_[i] == pid) return;
    }
    
    ready_queue_[ready_tail_] = pid;
    ready_tail_ = (ready_tail_ + 1) % MAX_PROCS;
}

void Scheduler::unready(xinim::pid_t pid) noexcept {
    int new_tail = ready_head_;
    for (int i = ready_head_; i != ready_tail_; i = (i + 1) % MAX_PROCS) {
        if (ready_queue_[i] != pid) {
            ready_queue_[new_tail] = ready_queue_[i];
            new_tail = (new_tail + 1) % MAX_PROCS;
        }
    }
    ready_tail_ = new_tail;
}

bool Scheduler::block_on(xinim::pid_t pid, xinim::pid_t target) noexcept {
    if (pid < 0 || pid >= MAX_PROCS) return false;
    
    unready(pid);
    waiting_[pid] = target;
    if (target != -1) {
        graph_.add_edge(pid, target);
    }
    return true;
}

void Scheduler::unblock(xinim::pid_t pid) noexcept {
    if (pid < 0 || pid >= MAX_PROCS) return;
    
    xinim::pid_t target = waiting_[pid];
    if (target != -1) {
        graph_.remove_edge(pid, target);
    }
    waiting_[pid] = -1;
    ready(pid);
}

void Scheduler::yield() noexcept {
    if (current_ != -1) {
        ready(current_);
    }
    pick_next();
}

void Scheduler::yield_to(xinim::pid_t pid) noexcept {
    if (current_ != -1) {
        ready(current_);
    }
    unready(pid);
    current_ = pid;
}

void Scheduler::handle_crash(xinim::pid_t pid) {
    // Note: service_manager might still use std containers, but we'll get to that.
    if (!svc::service_manager.handle_crash(pid) && current_ == pid) {
        current_ = -1;
    }
}

} // namespace sched

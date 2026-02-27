#include "lock_manager.hpp"
#include "schedule.hpp"

namespace xinim::sync {

LockManager lock_manager{};

void LockManager::register_lock(xinim::pid_t pid, CapabilityMutex* mutex) noexcept {
    if (lock_count_ < MAX_LOCKS) {
        locks_[lock_count_++] = {pid, mutex};
    }
}

void LockManager::unregister_lock(xinim::pid_t pid, CapabilityMutex* mutex) noexcept {
    for (int i = 0; i < lock_count_; ++i) {
        if (locks_[i].pid == pid && locks_[i].mutex == mutex) {
            locks_[i] = locks_[--lock_count_];
            return;
        }
    }
}

void LockManager::release_all_for_pid(xinim::pid_t pid) noexcept {
    for (int i = 0; i < lock_count_; ) {
        if (locks_[i].pid == pid) {
            locks_[i] = locks_[--lock_count_];
        } else {
            ++i;
        }
    }
}

bool LockManager::has_deadlocks() const noexcept {
    for (int i = 0; i < lock_count_; ++i) {
        if (sched::scheduler.graph().is_in_cycle(locks_[i].pid)) {
            return true;
        }
    }
    return false;
}

} // namespace xinim::sync

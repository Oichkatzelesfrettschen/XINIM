#include "lock_manager.hpp"

#include "schedule.hpp"

namespace xinim::sync {

    LockManager lock_manager{};

    void LockManager::register_lock(xinim::pid_t pid, CapabilityMutex *mutex) noexcept {
        if (lock_count_ < locks_.size()) {
            locks_[lock_count_++] = {pid, mutex};
        }
    }

    void LockManager::unregister_lock(xinim::pid_t pid, CapabilityMutex *mutex) noexcept {
        for (std::size_t lock_index = 0; lock_index < lock_count_; ++lock_index) {
            if (locks_[lock_index].pid == pid && locks_[lock_index].mutex == mutex) {
                locks_[lock_index] = locks_[--lock_count_];
                return;
            }
        }
    }

    void LockManager::release_all_for_pid(xinim::pid_t pid) noexcept {
        for (std::size_t lock_index = 0; lock_index < lock_count_;) {
            if (locks_[lock_index].pid == pid) {
                locks_[lock_index] = locks_[--lock_count_];
            } else {
                ++lock_index;
            }
        }
    }

    bool LockManager::has_deadlocks() const noexcept {
        for (std::size_t lock_index = 0; lock_index < lock_count_; ++lock_index) {
            if (sched::scheduler.graph().is_in_cycle(locks_[lock_index].pid)) {
                return true;
            }
        }
        return false;
    }

} // namespace xinim::sync

#include "capability_mutex.hpp"
#include "lock_manager.hpp"

namespace xinim::sync {

bool CapabilityMutex::lock(xinim::pid_t pid, const CapabilityToken& token) noexcept {
    if (!verify_token(token)) return false;

    bool expected = false;
    if (locked_.compare_exchange_strong(expected, true)) {
        owner_ = pid;
        lock_manager.register_lock(pid, this);
        return true;
    }

    if (wait_count_ < MAX_WAITERS) {
        wait_queue_[wait_count_++] = {pid, token.token_id};
        sched::scheduler.block_on(pid, owner_);
    }
    return false;
}

void CapabilityMutex::unlock(xinim::pid_t pid) noexcept {
    if (owner_ != pid) return;

    lock_manager.unregister_lock(pid, this);
    owner_ = -1;
    locked_ = false;

    if (wait_count_ > 0) {
        auto next = wait_queue_[0];
        for (std::size_t i = 0; i < wait_count_ - 1; ++i) {
            wait_queue_[i] = wait_queue_[i+1];
        }
        wait_count_--;
        
        // Handover logic would go here
        sched::scheduler.unblock(next.pid);
    }
}

void CapabilityMutex::force_unlock() noexcept {
    if (owner_ != -1) {
        lock_manager.unregister_lock(owner_, this);
    }
    owner_ = -1;
    locked_ = false;
    
    while (wait_count_ > 0) {
        sched::scheduler.unblock(wait_queue_[--wait_count_].pid);
    }
}

bool CapabilityMutex::verify_token(const CapabilityToken& token) const noexcept {
    static constexpr lattice::Octonion generator{{1, 0, 0, 0, 0, 0, 0, 0}};
    [[maybe_unused]] auto identity = lattice::fano_multiply(token.proof, generator);
    
    bool non_trivial = false;
    for (auto val : token.proof.comp) {
        if (val != 0) {
            non_trivial = true;
            break;
        }
    }
    return non_trivial;
}

} // namespace xinim::sync

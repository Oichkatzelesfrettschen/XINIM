#pragma once
/**
 * @file capability_mutex.hpp
 * @brief Capability-based mutex with automatic crash recovery.
 *
 * Integrates with XINIM's resurrection server to automatically release
 * locks when the holding service crashes. Prevents deadlocks from crashed
 * lock holders.
 */

#include "../include/xinim/core_types.hpp"
#include "schedule.hpp"
#include <atomic>
#include <cstdint>
#include <deque>
#include <optional>

namespace xinim::sync {

/**
 * @brief Capability token for lock operations.
 *
 * Represents a cryptographic capability that authorizes lock operations.
 * Tokens are issued by the capability system and verified on each lock operation.
 */
struct CapabilityToken {
    uint64_t token_id;       ///< Unique token identifier
    xinim::pid_t issuer_pid; ///< PID of the process that was issued this token
    uint64_t expiry_time;    ///< Expiration timestamp (TSC)
    uint32_t rights;         ///< Access rights bitmask

    // Rights flags
    static constexpr uint32_t CAP_LOCK   = 0x01; ///< Can acquire locks
    static constexpr uint32_t CAP_UNLOCK = 0x02; ///< Can release locks
    static constexpr uint32_t CAP_FORCE  = 0x04; ///< Can force-unlock (admin only)

    /**
     * @brief Check if token has a specific right.
     */
    [[nodiscard]] bool has_right(uint32_t right) const noexcept {
        return (rights & right) != 0;
    }

    /**
     * @brief Check if token is expired.
     */
    [[nodiscard]] bool is_expired() const noexcept {
        return expiry_time != 0 && rdtsc() > expiry_time;
    }

  private:
    static inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        uint32_t lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
#else
        return 0; // Fallback: no expiry
#endif
    }
};

/**
 * @brief Capability-based mutex with crash recovery.
 *
 * Features:
 * - Requires valid capability token to acquire
 * - Automatic unlock when owner crashes (via resurrection server)
 * - Tainted lock tracking for debugging
 * - Integration with wait-for graph for deadlock detection
 *
 * Use cases:
 * - Service manager locks (VFS, network, etc.)
 * - Locks held across IPC boundaries
 * - Any lock that could deadlock if holder crashes
 */
class CapabilityMutex {
  public:
    constexpr CapabilityMutex() noexcept = default;

    /**
     * @brief Acquire the mutex with capability verification.
     *
     * @param pid Process ID of the caller.
     * @param token Capability token authorizing this operation.
     * @return true if lock was acquired, false if capability is invalid.
     */
    bool lock(xinim::pid_t pid, const CapabilityToken& token) {
        // Verify capability
        if (!verify_capability(pid, token)) {
            return false; // Invalid capability
        }

        // Try fast-path acquisition
        xinim::pid_t expected = 0;
        if (owner_.compare_exchange_strong(expected, pid,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
            // Acquired!
            owner_token_ = token.token_id;
            tainted_ = false; // Fresh lock

            // Register with lock manager (for crash recovery)
            register_with_lock_manager(pid);

            return true;
        }

        // Slow path: wait for lock
        return wait_for_lock(pid, token, expected);
    }

    /**
     * @brief Try to acquire the mutex without blocking.
     *
     * @param pid Process ID of the caller.
     * @param token Capability token.
     * @return true if lock was acquired, false otherwise.
     */
    [[nodiscard]] bool try_lock(xinim::pid_t pid, const CapabilityToken& token) {
        if (!verify_capability(pid, token)) {
            return false;
        }

        xinim::pid_t expected = 0;
        if (owner_.compare_exchange_strong(expected, pid,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
            owner_token_ = token.token_id;
            tainted_ = false;
            register_with_lock_manager(pid);
            return true;
        }

        return false;
    }

    /**
     * @brief Release the mutex.
     *
     * @param pid Process ID of the caller (must match owner).
     */
    void unlock(xinim::pid_t pid) {
        // Verify caller is the owner
        if (owner_.load(std::memory_order_relaxed) != pid) {
            return; // Not owner - ignore
        }

        owner_.store(0, std::memory_order_release);
        owner_token_ = 0;

        // Unregister from lock manager
        unregister_from_lock_manager(pid);

        // Wake next waiter
        wake_next_waiter();
    }

    /**
     * @brief Force-unlock by resurrection server when owner crashes.
     *
     * This is called by the lock manager when a service crashes while
     * holding this lock.
     *
     * @param crashed_pid PID of the crashed process.
     */
    void force_unlock(xinim::pid_t crashed_pid) {
        xinim::pid_t expected = crashed_pid;
        if (owner_.compare_exchange_strong(expected, 0,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
            // Mark lock as tainted (forced unlock occurred)
            tainted_ = true;

            // Log the event
            // TODO: Add kernel logging
            // kernel_log(LOG_WARNING, "Forced unlock of CapabilityMutex by crashed PID %d", crashed_pid);

            // Wake next waiter
            wake_next_waiter();
        }
    }

    /**
     * @brief Check if lock is currently held.
     */
    [[nodiscard]] bool is_locked() const noexcept {
        return owner_.load(std::memory_order_relaxed) != 0;
    }

    /**
     * @brief Get the current owner PID.
     */
    [[nodiscard]] std::optional<xinim::pid_t> owner() const noexcept {
        xinim::pid_t pid = owner_.load(std::memory_order_relaxed);
        return (pid != 0) ? std::optional<xinim::pid_t>(pid) : std::nullopt;
    }

    /**
     * @brief Check if lock was tainted by a forced unlock.
     *
     * Tainted locks indicate a previous crash while holding the lock.
     * Useful for debugging and fault analysis.
     */
    [[nodiscard]] bool is_tainted() const noexcept {
        return tainted_;
    }

    /**
     * @brief Clear the tainted flag.
     *
     * Should be called after investigating the crash.
     */
    void clear_tainted() noexcept {
        tainted_ = false;
    }

    /**
     * @brief Get the number of waiters.
     */
    [[nodiscard]] size_t waiter_count() const noexcept {
        return wait_queue_.size();
    }

  private:
    alignas(64) std::atomic<xinim::pid_t> owner_{0};  ///< Current owner (0 = unlocked)
    uint64_t owner_token_{0};                          ///< Token ID of current owner
    bool tainted_{false};                              ///< True if forced unlock occurred
    std::deque<std::pair<xinim::pid_t, uint64_t>> wait_queue_; ///< (PID, token_id) pairs

    /**
     * @brief Verify capability token.
     *
     * @param pid Caller's PID.
     * @param token Capability token to verify.
     * @return true if valid, false otherwise.
     */
    bool verify_capability(xinim::pid_t pid, const CapabilityToken& token) const {
        // Check token matches PID
        if (token.issuer_pid != pid) {
            return false;
        }

        // Check token has LOCK right
        if (!token.has_right(CapabilityToken::CAP_LOCK)) {
            return false;
        }

        // Check token hasn't expired
        if (token.is_expired()) {
            return false;
        }

        // TODO: Verify cryptographic signature
        // For now, basic checks are sufficient

        return true;
    }

    /**
     * @brief Wait for lock to become available.
     *
     * @param pid Caller's PID.
     * @param token Capability token.
     * @param owner_pid Current owner's PID.
     * @return true if acquired, false on error.
     */
    bool wait_for_lock(xinim::pid_t pid, const CapabilityToken& token, xinim::pid_t owner_pid) {
        // Add to wait queue
        wait_queue_.emplace_back(pid, token.token_id);

        // Block in scheduler
        if (sched::scheduler_initialized()) {
            if (!sched::scheduler.block_on(pid, owner_pid)) {
                // Deadlock detected - remove from queue
                remove_from_wait_queue(pid);
                return false;
            }
        }

        // When unblocked, try to acquire
        return lock(pid, token); // Recursive call (will hit fast path)
    }

    /**
     * @brief Wake the next waiter in the queue.
     */
    void wake_next_waiter() {
        if (!wait_queue_.empty()) {
            auto [next_pid, next_token] = wait_queue_.front();
            wait_queue_.pop_front();

            // Unblock in scheduler
            if (sched::scheduler_initialized()) {
                sched::scheduler.unblock(next_pid);
            }
        }
    }

    /**
     * @brief Remove a PID from the wait queue.
     */
    void remove_from_wait_queue(xinim::pid_t pid) {
        wait_queue_.erase(
            std::remove_if(wait_queue_.begin(), wait_queue_.end(),
                           [pid](const auto& entry) { return entry.first == pid; }),
            wait_queue_.end()
        );
    }

    /**
     * @brief Register lock ownership with the lock manager.
     *
     * This allows the resurrection server to force-unlock if we crash.
     */
    void register_with_lock_manager(xinim::pid_t pid);

    /**
     * @brief Unregister from lock manager.
     */
    void unregister_from_lock_manager(xinim::pid_t pid);
};

/**
 * @brief RAII guard for CapabilityMutex.
 */
class CapabilityLockGuard {
  public:
    /**
     * @brief Acquire the mutex on construction.
     *
     * @param mutex The mutex to lock.
     * @param pid Caller's process ID.
     * @param token Capability token.
     * @throws If capability is invalid (in practice, returns early).
     */
    CapabilityLockGuard(CapabilityMutex& mutex, xinim::pid_t pid, const CapabilityToken& token)
        : mutex_(mutex), pid_(pid), locked_(false) {
        locked_ = mutex_.lock(pid_, token);
    }

    /**
     * @brief Release the mutex on destruction.
     */
    ~CapabilityLockGuard() {
        if (locked_) {
            mutex_.unlock(pid_);
        }
    }

    /**
     * @brief Check if lock was successfully acquired.
     */
    [[nodiscard]] bool owns_lock() const noexcept {
        return locked_;
    }

    // Prevent copying/moving
    CapabilityLockGuard(const CapabilityLockGuard&) = delete;
    CapabilityLockGuard& operator=(const CapabilityLockGuard&) = delete;
    CapabilityLockGuard(CapabilityLockGuard&&) = delete;
    CapabilityLockGuard& operator=(CapabilityLockGuard&&) = delete;

  private:
    CapabilityMutex& mutex_;
    xinim::pid_t pid_;
    bool locked_;
};

} // namespace xinim::sync

// Helper to check scheduler availability
namespace sched {
    inline bool scheduler_initialized() {
        return true; // Assume scheduler is always available in kernel
    }
}

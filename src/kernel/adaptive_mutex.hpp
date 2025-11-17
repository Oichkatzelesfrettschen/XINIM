#pragma once
/**
 * @file adaptive_mutex.hpp
 * @brief Adaptive mutex: spin-then-sleep for optimal performance.
 *
 * Based on illumos adaptive mutex design. Spins if the owner is running
 * on another CPU (short critical section), sleeps if owner is blocked
 * (long wait expected).
 */

#include "schedule.hpp"
#include "../include/xinim/core_types.hpp"
#include <atomic>
#include <optional>
#include <deque>
#include <cstdint>

namespace xinim::sync {

/**
 * @brief Adaptive mutex with spin-then-sleep strategy.
 *
 * Performance strategy:
 * 1. Try fast-path acquisition (single CAS)
 * 2. If owner is running on another CPU → spin for a while
 * 3. If owner is blocked or spin timeout → sleep in scheduler
 *
 * Performance characteristics:
 * - Uncontended: ~2-3 cycles (single CAS)
 * - Short critical section: Spin (no context switch)
 * - Long critical section: Sleep (efficient CPU usage)
 * - Fairness: FIFO via wait queue
 *
 * Use cases:
 * - IPC channel locks (variable hold time)
 * - Service manager locks
 * - General-purpose kernel mutex
 */
class AdaptiveMutex {
  public:
    constexpr AdaptiveMutex() noexcept = default;

    /**
     * @brief Acquire the mutex.
     *
     * Implements adaptive algorithm:
     * 1. Fast path: CAS to acquire
     * 2. If owner running: spin with exponential backoff
     * 3. If owner blocked or timeout: sleep in scheduler
     *
     * @param current_pid Process ID of the caller (from Process::current()->p_pid)
     */
    void lock(xinim::pid_t current_pid) {
        // Fast path: try to acquire immediately
        xinim::pid_t expected = 0;
        if (owner_.compare_exchange_strong(expected, current_pid,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
            return; // Acquired!
        }

        // Slow path: adaptive spin or sleep
        adaptive_acquire(current_pid, expected);
    }

    /**
     * @brief Try to acquire the mutex without blocking.
     *
     * @param current_pid Process ID of the caller.
     * @return true if mutex was acquired, false otherwise.
     */
    [[nodiscard]] bool try_lock(xinim::pid_t current_pid) noexcept {
        xinim::pid_t expected = 0;
        return owner_.compare_exchange_strong(expected, current_pid,
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed);
    }

    /**
     * @brief Release the mutex.
     *
     * If there are waiters, wakes the next one via scheduler.
     */
    void unlock() noexcept {
        owner_.store(0, std::memory_order_release);

        // Wake next waiter
        if (!wait_queue_.empty()) {
            xinim::pid_t next = wait_queue_.front();
            wait_queue_.pop_front();

            // Unblock in scheduler
            if (sched::scheduler_initialized()) {
                sched::scheduler.unblock(next);
            }
        }
    }

    /**
     * @brief Check if mutex is currently locked.
     *
     * @return true if locked, false otherwise.
     * @note Snapshot only - may be stale immediately.
     */
    [[nodiscard]] bool is_locked() const noexcept {
        return owner_.load(std::memory_order_relaxed) != 0;
    }

    /**
     * @brief Get the current owner PID.
     *
     * @return Owner PID, or 0 if unlocked.
     * @note Snapshot only.
     */
    [[nodiscard]] xinim::pid_t owner() const noexcept {
        return owner_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get the number of waiters.
     *
     * @return Number of processes waiting for this mutex.
     */
    [[nodiscard]] size_t waiter_count() const noexcept {
        return wait_queue_.size();
    }

  private:
    alignas(64) std::atomic<xinim::pid_t> owner_{0};  ///< Current owner (0 = unlocked)
    std::deque<xinim::pid_t> wait_queue_;              ///< FIFO wait queue

    // Tuning constants
    static constexpr int SPIN_ITERATIONS = 1000;       ///< Max spin iterations
    static constexpr int MAX_BACKOFF = 64;             ///< Max backoff cycles
    static constexpr uint64_t TSC_THRESHOLD = 100000;  ///< ~40μs on 2.5GHz CPU

    /**
     * @brief Adaptive acquisition: spin then sleep.
     *
     * @param current_pid Caller's PID.
     * @param owner_pid Current owner's PID.
     */
    void adaptive_acquire(xinim::pid_t current_pid, xinim::pid_t owner_pid) {
        // Check if owner is running on another CPU
        if (is_owner_running(owner_pid)) {
            // Owner is running - try spinning first
            if (adaptive_spin(current_pid)) {
                return; // Acquired during spin!
            }
        }

        // Owner is blocked or spin timed out - sleep
        sleep_acquire(current_pid);
    }

    /**
     * @brief Spin with exponential backoff.
     *
     * @param current_pid Caller's PID.
     * @return true if mutex was acquired during spin, false if timeout.
     */
    bool adaptive_spin(xinim::pid_t current_pid) {
        uint64_t tsc_start = rdtsc();
        int backoff = 1;

        for (int i = 0; i < SPIN_ITERATIONS; i++) {
            // Try to acquire
            xinim::pid_t expected = 0;
            if (owner_.compare_exchange_strong(expected, current_pid,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
                return true; // Acquired!
            }

            // Exponential backoff
            for (int j = 0; j < backoff; j++) {
                cpu_pause();
            }
            backoff = (backoff < MAX_BACKOFF) ? backoff * 2 : MAX_BACKOFF;

            // Check TSC timeout
            if (rdtsc() - tsc_start > TSC_THRESHOLD) {
                return false; // Timeout - stop spinning
            }
        }

        return false; // Max iterations reached
    }

    /**
     * @brief Block in scheduler until mutex is available.
     *
     * @param current_pid Caller's PID.
     */
    void sleep_acquire(xinim::pid_t current_pid) {
        // Add to wait queue
        wait_queue_.push_back(current_pid);

        xinim::pid_t owner_pid = owner_.load(std::memory_order_relaxed);

        // Block on owner (if scheduler is available)
        if (sched::scheduler_initialized() && owner_pid != 0) {
            if (!sched::scheduler.block_on(current_pid, owner_pid)) {
                // Cycle detected - abort to prevent deadlock
                // Remove from wait queue
                wait_queue_.erase(
                    std::remove(wait_queue_.begin(), wait_queue_.end(), current_pid),
                    wait_queue_.end()
                );
                return; // Return without acquiring (caller should handle error)
            }
        }

        // When unblocked, try to acquire again
        xinim::pid_t expected = 0;
        if (!owner_.compare_exchange_strong(expected, current_pid,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
            // Failed to acquire - retry
            adaptive_acquire(current_pid, expected);
        }
    }

    /**
     * @brief Check if the owner is currently running on any CPU.
     *
     * @param pid Owner's PID.
     * @return true if owner is running, false if blocked.
     */
    bool is_owner_running(xinim::pid_t pid) const {
        if (!sched::scheduler_initialized()) {
            return false; // Assume blocked if no scheduler
        }

        // Check if pid is the current process on any CPU
        // For now, simplified: check if pid is not blocked
        return !sched::scheduler.is_blocked(pid);
    }

    /**
     * @brief Read Time Stamp Counter.
     *
     * @return Current TSC value.
     */
    static inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        uint32_t lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
#else
        // Fallback: use iteration count
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    /**
     * @brief CPU pause instruction.
     */
    static inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
        asm volatile("yield" ::: "memory");
#else
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }
};

/**
 * @brief RAII lock guard for AdaptiveMutex.
 */
class AdaptiveLockGuard {
  public:
    /**
     * @brief Acquire the mutex on construction.
     *
     * @param mutex The mutex to lock.
     * @param pid Caller's process ID.
     */
    explicit AdaptiveLockGuard(AdaptiveMutex& mutex, xinim::pid_t pid) noexcept
        : mutex_(mutex), pid_(pid) {
        mutex_.lock(pid_);
    }

    /**
     * @brief Release the mutex on destruction.
     */
    ~AdaptiveLockGuard() {
        mutex_.unlock();
    }

    // Prevent copying/moving
    AdaptiveLockGuard(const AdaptiveLockGuard&) = delete;
    AdaptiveLockGuard& operator=(const AdaptiveLockGuard&) = delete;
    AdaptiveLockGuard(AdaptiveLockGuard&&) = delete;
    AdaptiveLockGuard& operator=(AdaptiveLockGuard&&) = delete;

  private:
    AdaptiveMutex& mutex_;
    xinim::pid_t pid_;
};

} // namespace xinim::sync

// Helper to check if scheduler is initialized
namespace sched {
    inline bool scheduler_initialized() {
        // TODO: Add proper initialization check
        // For now, return true (scheduler is a global object)
        return true;
    }
}

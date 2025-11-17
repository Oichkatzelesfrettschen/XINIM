#pragma once
/**
 * @file ticket_spinlock.hpp
 * @brief Ticket-based spinlock with FIFO fairness guarantees.
 *
 * Implements the Mellor-Crummey & Scott ticket lock algorithm.
 * Provides strict FIFO ordering to prevent starvation.
 */

#include <atomic>
#include <cstdint>

namespace xinim::sync {

/**
 * @brief FIFO-fair spinlock using ticket algorithm.
 *
 * Each thread takes a ticket number and waits until its number is called.
 * Guarantees fairness: threads acquire the lock in the order they requested it.
 *
 * Performance characteristics:
 * - Uncontended: ~1-2 cycles (single fetch-add)
 * - Contended: Better than TAS spinlock (reduces cache-line bouncing)
 * - Fairness: Strict FIFO (no starvation)
 * - Memory: 8 bytes (2x uint32_t)
 *
 * Use cases:
 * - Short critical sections (<10μs)
 * - Fair resource allocation
 * - Kernel heap allocator
 * - Global data structure locks
 */
class TicketSpinlock {
  public:
    constexpr TicketSpinlock() noexcept = default;

    /**
     * @brief Acquire the lock.
     *
     * Takes a ticket and spins until the ticket is called.
     * Memory ordering: acquire (synchronizes with unlock's release)
     */
    void lock() noexcept {
        const uint32_t my_ticket = next_ticket_.fetch_add(1, std::memory_order_relaxed);

        // Spin until our ticket is called
        while (now_serving_.load(std::memory_order_acquire) != my_ticket) {
            cpu_pause();
        }

        // Lock acquired! Memory fence established by acquire load
    }

    /**
     * @brief Try to acquire the lock without blocking.
     *
     * @return true if lock was acquired, false otherwise.
     */
    [[nodiscard]] bool try_lock() noexcept {
        uint32_t serving = now_serving_.load(std::memory_order_acquire);
        uint32_t next = next_ticket_.load(std::memory_order_relaxed);

        if (serving == next) {
            // No one else has a ticket - try to get one
            return next_ticket_.compare_exchange_strong(next, next + 1,
                                                         std::memory_order_acquire,
                                                         std::memory_order_relaxed);
        }

        return false; // Someone else has a ticket
    }

    /**
     * @brief Release the lock.
     *
     * Increments the serving counter to allow the next ticket holder to proceed.
     * Memory ordering: release (synchronizes with lock's acquire)
     */
    void unlock() noexcept {
        // Call the next ticket
        now_serving_.fetch_add(1, std::memory_order_release);
    }

    /**
     * @brief Check if the lock is currently held.
     *
     * @return true if lock is held, false otherwise.
     *
     * @note This is a snapshot and may be immediately stale.
     * Only use for debugging/statistics, not for synchronization.
     */
    [[nodiscard]] bool is_locked() const noexcept {
        return now_serving_.load(std::memory_order_relaxed) !=
               next_ticket_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get the current queue length (approximate).
     *
     * @return Number of threads waiting for the lock.
     *
     * @note Racy: The value may be stale immediately after return.
     */
    [[nodiscard]] uint32_t queue_length() const noexcept {
        uint32_t serving = now_serving_.load(std::memory_order_relaxed);
        uint32_t next = next_ticket_.load(std::memory_order_relaxed);
        return (next > serving) ? (next - serving) : 0;
    }

  private:
    alignas(64) std::atomic<uint32_t> next_ticket_{0};    ///< Next ticket to dispense
    alignas(64) std::atomic<uint32_t> now_serving_{0};    ///< Ticket currently being served

    /**
     * @brief CPU pause instruction for spin-wait loops.
     *
     * Reduces power consumption and improves performance on SMT/hyper-threading CPUs.
     * On x86: PAUSE instruction (REP NOP)
     * On ARM: YIELD instruction
     */
    static inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
        asm volatile("yield" ::: "memory");
#else
        // Generic: memory barrier
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }
};

/**
 * @brief RAII lock guard for TicketSpinlock.
 *
 * Acquires the lock on construction, releases on destruction.
 * Cannot be copied or moved (prevent accidental early unlock).
 */
class TicketLockGuard {
  public:
    explicit TicketLockGuard(TicketSpinlock& lock) noexcept
        : lock_(lock) {
        lock_.lock();
    }

    ~TicketLockGuard() {
        lock_.unlock();
    }

    // Prevent copying/moving
    TicketLockGuard(const TicketLockGuard&) = delete;
    TicketLockGuard& operator=(const TicketLockGuard&) = delete;
    TicketLockGuard(TicketLockGuard&&) = delete;
    TicketLockGuard& operator=(TicketLockGuard&&) = delete;

  private:
    TicketSpinlock& lock_;
};

} // namespace xinim::sync

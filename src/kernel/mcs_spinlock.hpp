#pragma once
/**
 * @file mcs_spinlock.hpp
 * @brief MCS (Mellor-Crummey & Scott) queue-based spinlock.
 *
 * Implements the MCS lock algorithm for scalable, NUMA-aware synchronization.
 * Each waiting thread spins on its own cache line, eliminating cache-line bouncing.
 */

#include <atomic>
#include <cstdint>

namespace xinim::sync {

/**
 * @brief Per-thread queue node for MCS lock.
 *
 * Each thread allocates one of these (typically thread-local storage)
 * and uses it when acquiring the lock.
 */
struct MCSNode {
    std::atomic<MCSNode*> next{nullptr};  ///< Next node in queue
    std::atomic<bool> locked{false};      ///< True if this node is waiting

    MCSNode() = default;
};

/**
 * @brief Scalable MCS queue-based spinlock.
 *
 * Unlike simple spinlocks where all waiters spin on the same memory location
 * (causing cache-line bouncing), MCS lock organizes waiters in a queue where
 * each thread spins on its own node.
 *
 * Performance characteristics:
 * - Uncontended: ~4-5 cycles (exchange + null check)
 * - Contended: Excellent scalability (no cache-line bouncing)
 * - Fairness: Strict FIFO
 * - Memory: 8 bytes (lock) + 16 bytes per waiting thread (node)
 *
 * Advantages:
 * - ✅ Scalable to 128+ CPUs
 * - ✅ NUMA-friendly (local spinning)
 * - ✅ No cache-line thrashing
 * - ✅ FIFO fairness
 *
 * Disadvantages:
 * - ❌ Requires per-thread storage (MCSNode)
 * - ❌ More complex than ticket lock
 * - ❌ Unlock is expensive if no successor
 *
 * Use cases:
 * - High-contention locks (process table, VFS cache)
 * - Large SMP systems (8+ CPUs)
 * - NUMA systems
 */
class MCSSpinlock {
  public:
    constexpr MCSSpinlock() noexcept = default;

    /**
     * @brief Acquire the lock using the provided node.
     *
     * The caller must provide a node (typically from thread-local storage).
     * The node must remain valid until unlock() is called.
     *
     * @param my_node Pointer to the caller's queue node.
     *
     * Memory ordering:
     * - Exchange: acquire (synchronizes with previous unlock)
     * - Load: acquire (synchronizes with predecessor's next store)
     */
    void lock(MCSNode* my_node) noexcept {
        // Initialize our node
        my_node->next.store(nullptr, std::memory_order_relaxed);
        my_node->locked.store(true, std::memory_order_relaxed);

        // Enqueue ourselves (atomic exchange returns previous tail)
        MCSNode* prev = tail_.exchange(my_node, std::memory_order_acquire);

        if (prev != nullptr) {
            // There's a predecessor - link to them and wait
            prev->next.store(my_node, std::memory_order_release);

            // Spin on our own node (no cache-line bouncing!)
            while (my_node->locked.load(std::memory_order_acquire)) {
                cpu_pause();
            }
        }

        // Lock acquired! (either we were first, or predecessor unlocked us)
    }

    /**
     * @brief Try to acquire the lock without blocking.
     *
     * @param my_node Pointer to the caller's queue node.
     * @return true if lock was acquired, false otherwise.
     */
    [[nodiscard]] bool try_lock(MCSNode* my_node) noexcept {
        my_node->next.store(nullptr, std::memory_order_relaxed);
        my_node->locked.store(false, std::memory_order_relaxed);

        // Try to set ourselves as tail if currently null
        MCSNode* expected = nullptr;
        return tail_.compare_exchange_strong(expected, my_node,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed);
    }

    /**
     * @brief Release the lock.
     *
     * @param my_node Pointer to the node used in lock() call.
     *
     * Memory ordering:
     * - Load: acquire (synchronizes with successor's next store)
     * - CAS: release (synchronizes with next lock() call)
     * - Store: release (synchronizes with successor's locked load)
     */
    void unlock(MCSNode* my_node) noexcept {
        // Check if there's a successor
        MCSNode* successor = my_node->next.load(std::memory_order_acquire);

        if (successor == nullptr) {
            // We might be last in queue - try to remove ourselves
            if (tail_.compare_exchange_strong(my_node, nullptr,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                return; // We were last - done!
            }

            // Someone enqueued after we checked - wait for them to link
            while ((successor = my_node->next.load(std::memory_order_acquire)) == nullptr) {
                cpu_pause();
            }
        }

        // Unlock the successor
        successor->locked.store(false, std::memory_order_release);
    }

    /**
     * @brief Check if the lock is currently held (approximate).
     *
     * @return true if tail is non-null (someone is queued).
     *
     * @note This is a snapshot and may be stale immediately.
     */
    [[nodiscard]] bool is_locked() const noexcept {
        return tail_.load(std::memory_order_relaxed) != nullptr;
    }

  private:
    alignas(64) std::atomic<MCSNode*> tail_{nullptr};  ///< Tail of the queue

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
 * @brief RAII lock guard for MCSSpinlock.
 *
 * Automatically allocates a node (thread-local storage recommended),
 * acquires the lock on construction, releases on destruction.
 */
class MCSLockGuard {
  public:
    explicit MCSLockGuard(MCSSpinlock& lock) noexcept
        : lock_(lock), node_() {
        lock_.lock(&node_);
    }

    ~MCSLockGuard() {
        lock_.unlock(&node_);
    }

    // Prevent copying/moving
    MCSLockGuard(const MCSLockGuard&) = delete;
    MCSLockGuard& operator=(const MCSLockGuard&) = delete;
    MCSLockGuard(MCSLockGuard&&) = delete;
    MCSLockGuard& operator=(MCSLockGuard&&) = delete;

  private:
    MCSSpinlock& lock_;
    MCSNode node_;  // Per-instance node (could be optimized with thread_local)
};

/**
 * @brief Thread-local MCS node pool.
 *
 * Provides efficient per-thread node allocation for MCS locks.
 * Avoids dynamic allocation overhead.
 */
class MCSNodePool {
  public:
    /**
     * @brief Get a free node from the pool.
     *
     * @return Pointer to a node (guaranteed non-null).
     */
    static MCSNode* acquire_node() noexcept {
        thread_local MCSNode node;
        return &node;
    }

    // No need to release (thread-local lifetime management)
};

} // namespace xinim::sync

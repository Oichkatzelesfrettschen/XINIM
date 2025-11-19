#pragma once
/**
 * @file lock_manager.hpp
 * @brief Lock manager for automatic crash recovery.
 *
 * Tracks all capability-based locks held by services. When a service crashes,
 * the resurrection server calls the lock manager to force-release all locks
 * held by the crashed service.
 */

#include "../include/xinim/core_types.hpp"
#include "capability_mutex.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>

namespace xinim::sync {

/**
 * @brief Global lock manager for crash recovery.
 *
 * Responsibilities:
 * - Track which process holds which locks
 * - Force-release all locks when a process crashes
 * - Provide lock statistics for debugging
 *
 * Integration:
 * - Called by CapabilityMutex::lock() to register ownership
 * - Called by CapabilityMutex::unlock() to unregister ownership
 * - Called by ServiceManager::handle_crash() to force-release locks
 */
class LockManager {
  public:
    LockManager() = default;

    /**
     * @brief Register a lock held by a process.
     *
     * Called automatically by CapabilityMutex::lock().
     *
     * @param pid Process ID of the lock holder.
     * @param lock Pointer to the capability mutex.
     */
    void register_lock(xinim::pid_t pid, CapabilityMutex* lock) {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        auto& locks = held_locks_[pid];

        // Add lock if not already present
        if (std::find(locks.begin(), locks.end(), lock) == locks.end()) {
            locks.push_back(lock);
        }

        // Update statistics
        total_locks_acquired_++;
    }

    /**
     * @brief Unregister a lock (no longer held).
     *
     * Called automatically by CapabilityMutex::unlock().
     *
     * @param pid Process ID of the former lock holder.
     * @param lock Pointer to the capability mutex.
     */
    void unregister_lock(xinim::pid_t pid, CapabilityMutex* lock) {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        auto it = held_locks_.find(pid);
        if (it != held_locks_.end()) {
            auto& locks = it->second;
            locks.erase(std::remove(locks.begin(), locks.end(), lock), locks.end());

            // Clean up empty entries
            if (locks.empty()) {
                held_locks_.erase(it);
            }
        }

        // Update statistics
        total_locks_released_++;
    }

    /**
     * @brief Handle service crash: force-release all locks.
     *
     * Called by the resurrection server when a service crashes.
     *
     * @param crashed_pid PID of the crashed service.
     * @return Number of locks that were force-released.
     */
    size_t handle_crash(xinim::pid_t crashed_pid) {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        auto it = held_locks_.find(crashed_pid);
        if (it == held_locks_.end()) {
            return 0; // Process held no locks
        }

        auto& locks = it->second;
        size_t count = locks.size();

        // Force-unlock all locks held by the crashed process
        for (auto* lock : locks) {
            lock->force_unlock(crashed_pid);
        }

        // Remove entry
        held_locks_.erase(it);

        // Update statistics
        total_crashes_handled_++;
        total_locks_force_released_ += count;

        return count;
    }

    /**
     * @brief Get the number of locks held by a process.
     *
     * @param pid Process ID to query.
     * @return Number of locks currently held.
     */
    [[nodiscard]] size_t lock_count(xinim::pid_t pid) const {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        auto it = held_locks_.find(pid);
        return (it != held_locks_.end()) ? it->second.size() : 0;
    }

    /**
     * @brief Get all locks held by a process (for debugging).
     *
     * @param pid Process ID to query.
     * @return Vector of lock pointers.
     */
    [[nodiscard]] std::vector<CapabilityMutex*> get_locks(xinim::pid_t pid) const {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        auto it = held_locks_.find(pid);
        return (it != held_locks_.end()) ? it->second : std::vector<CapabilityMutex*>{};
    }

    /**
     * @brief Get the total number of active locks in the system.
     *
     * @return Total lock count across all processes.
     */
    [[nodiscard]] size_t total_active_locks() const {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        size_t total = 0;
        for (const auto& [pid, locks] : held_locks_) {
            total += locks.size();
        }
        return total;
    }

    /**
     * @brief Get the number of processes holding locks.
     *
     * @return Number of processes with active locks.
     */
    [[nodiscard]] size_t process_count() const {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);
        return held_locks_.size();
    }

    /**
     * @brief Get lock statistics.
     */
    struct Statistics {
        uint64_t total_acquired;       ///< Total locks acquired (lifetime)
        uint64_t total_released;       ///< Total locks released (lifetime)
        uint64_t total_force_released; ///< Total locks force-released due to crashes
        uint64_t total_crashes;        ///< Total crashes handled
        size_t active_locks;           ///< Current active lock count
        size_t active_processes;       ///< Current processes with locks
    };

    [[nodiscard]] Statistics get_statistics() const {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        return Statistics{
            .total_acquired = total_locks_acquired_,
            .total_released = total_locks_released_,
            .total_force_released = total_locks_force_released_,
            .total_crashes = total_crashes_handled_,
            .active_locks = total_active_locks(),
            .active_processes = held_locks_.size()
        };
    }

    /**
     * @brief Reset statistics (for testing).
     */
    void reset_statistics() {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        total_locks_acquired_ = 0;
        total_locks_released_ = 0;
        total_locks_force_released_ = 0;
        total_crashes_handled_ = 0;
    }

    /**
     * @brief Detect potential deadlocks.
     *
     * Scans all processes holding locks and checks if any are waiting
     * on locks held by others (cycle detection).
     *
     * @return Vector of PIDs involved in potential deadlocks.
     */
    [[nodiscard]] std::vector<xinim::pid_t> detect_deadlocks() const {
        std::lock_guard<std::mutex> guard(lock_map_mutex_);

        std::vector<xinim::pid_t> deadlocked_pids;

        // TODO: Implement cycle detection in lock wait graph
        // For now, return empty (no deadlocks detected)

        return deadlocked_pids;
    }

  private:
    mutable std::mutex lock_map_mutex_;  ///< Protects held_locks_ map

    /// Map: PID -> List of locks held
    std::unordered_map<xinim::pid_t, std::vector<CapabilityMutex*>> held_locks_;

    // Statistics
    uint64_t total_locks_acquired_{0};
    uint64_t total_locks_released_{0};
    uint64_t total_locks_force_released_{0};
    uint64_t total_crashes_handled_{0};
};

/// Global lock manager instance
extern LockManager lock_manager;

} // namespace xinim::sync

// Implementation of CapabilityMutex hooks
namespace xinim::sync {

inline void CapabilityMutex::register_with_lock_manager(xinim::pid_t pid) {
    lock_manager.register_lock(pid, this);
}

inline void CapabilityMutex::unregister_from_lock_manager(xinim::pid_t pid) {
    lock_manager.unregister_lock(pid, this);
}

} // namespace xinim::sync

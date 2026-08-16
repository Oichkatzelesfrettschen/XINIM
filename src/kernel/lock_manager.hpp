#pragma once
/**
 * @file lock_manager.hpp
 * @brief Lock manager for bare-metal environment (Fixed-size).
 */

#include "../include/xinim/core_types.hpp"

#include <array>
#include <cstddef>

namespace xinim::sync {

    class CapabilityMutex;

    /**
     * @brief Global lock manager tracking mutex ownership.
     */
    class LockManager {
    public:
        static constexpr int MAX_LOCKS = 128;
        static constexpr int MAX_PROCS = 64;

        LockManager() = default;

        /** @brief Register a process as holding a lock. */
        void register_lock(xinim::pid_t pid, CapabilityMutex *mutex) noexcept;

        /** @brief Unregister a lock from a process. */
        void unregister_lock(xinim::pid_t pid, CapabilityMutex *mutex) noexcept;

        /** @brief Release all locks held by a process (on crash). */
        void release_all_for_pid(xinim::pid_t pid) noexcept;

        /** @brief Detect if any deadlocks exist. */
        [[nodiscard]] bool has_deadlocks() const noexcept;

    private:
        struct LockEntry {
            xinim::pid_t pid{-1};
            CapabilityMutex *mutex{nullptr};
        };

        static constexpr std::size_t kLockCapacity = static_cast<std::size_t>(MAX_LOCKS);
        std::array<LockEntry, kLockCapacity> locks_{};
        std::size_t lock_count_{0};
    };

    extern LockManager lock_manager;

} // namespace xinim::sync

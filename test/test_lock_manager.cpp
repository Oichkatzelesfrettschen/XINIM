/**
 * @file test_lock_manager.cpp
 * @brief Unit tests for xinim::sync::LockManager.
 *
 * Tests register/unregister/release_all_for_pid operations.
 * Note: has_deadlocks() depends on sched::scheduler global and is
 * tested indirectly via test_capability_mutex.
 */

#include "lock_manager.hpp"
#include "capability_mutex.hpp"
#include <cassert>

using xinim::sync::LockManager;
using xinim::sync::CapabilityMutex;

static void test_register_unregister() {
    LockManager lm;
    CapabilityMutex m1, m2;

    lm.register_lock(1, &m1);
    lm.register_lock(1, &m2);
    lm.register_lock(2, &m1);

    // Unregister one -- should succeed without crash
    lm.unregister_lock(1, &m1);

    // Unregister non-existent -- should be a no-op
    lm.unregister_lock(99, &m1);

    // Double unregister -- should be a no-op
    lm.unregister_lock(1, &m1);
}

static void test_release_all_for_pid() {
    LockManager lm;
    CapabilityMutex m1, m2, m3;

    // PID 1 holds m1 and m2; PID 2 holds m3
    lm.register_lock(1, &m1);
    lm.register_lock(1, &m2);
    lm.register_lock(2, &m3);

    // Release all for PID 1 -- should remove m1 and m2 but keep m3
    lm.release_all_for_pid(1);

    // Unregistering m3 for PID 2 should still work (it was not released)
    lm.unregister_lock(2, &m3);

    // Release for a PID with no locks -- should be a no-op
    lm.release_all_for_pid(99);
}

static void test_capacity_limit() {
    LockManager lm;
    CapabilityMutex mutexes[LockManager::MAX_LOCKS + 10];

    // Fill to capacity
    for (int i = 0; i < LockManager::MAX_LOCKS; ++i) {
        lm.register_lock(static_cast<xinim::pid_t>(i % 10), &mutexes[i]);
    }

    // Beyond capacity should be silently ignored (no crash)
    lm.register_lock(1, &mutexes[LockManager::MAX_LOCKS]);
    lm.register_lock(2, &mutexes[LockManager::MAX_LOCKS + 1]);

    // Release should clean up correctly
    for (int pid = 0; pid < 10; ++pid) {
        lm.release_all_for_pid(static_cast<xinim::pid_t>(pid));
    }
}

static void test_same_mutex_different_pids() {
    LockManager lm;
    CapabilityMutex m;

    // Multiple PIDs can be registered with the same mutex pointer
    // (represents queued waiters in real usage)
    lm.register_lock(1, &m);
    lm.register_lock(2, &m);
    lm.register_lock(3, &m);

    // Releasing PID 2 should only remove that entry
    lm.release_all_for_pid(2);

    // PID 1 and 3 entries should remain -- unregister them individually
    lm.unregister_lock(1, &m);
    lm.unregister_lock(3, &m);
}

int main() {
    test_register_unregister();
    test_release_all_for_pid();
    test_capacity_limit();
    test_same_mutex_different_pids();
    return 0;
}

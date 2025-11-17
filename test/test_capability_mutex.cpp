/**
 * @file test_capability_mutex.cpp
 * @brief Unit tests for CapabilityMutex and LockManager.
 */

#include "../src/kernel/capability_mutex.hpp"
#include "../src/kernel/lock_manager.hpp"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>

using namespace xinim::sync;

// Mock PID counter
static std::atomic<xinim::pid_t> next_pid{1};

static xinim::pid_t get_test_pid() {
    return next_pid.fetch_add(1, std::memory_order_relaxed);
}

// Helper to create a valid capability token
static CapabilityToken create_token(xinim::pid_t pid) {
    return CapabilityToken{
        .token_id = static_cast<uint64_t>(pid) << 32 | 0xDEADBEEF,
        .issuer_pid = pid,
        .expiry_time = 0, // Never expires
        .rights = CapabilityToken::CAP_LOCK | CapabilityToken::CAP_UNLOCK
    };
}

/**
 * @brief Test basic lock/unlock with capabilities.
 */
static void test_basic_capability_lock() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    assert(!mutex.is_locked());

    // Lock with valid capability
    assert(mutex.lock(pid, token));
    assert(mutex.is_locked());
    assert(mutex.owner() == pid);

    // Unlock
    mutex.unlock(pid);
    assert(!mutex.is_locked());
}

/**
 * @brief Test capability verification.
 */
static void test_capability_verification() {
    CapabilityMutex mutex;
    xinim::pid_t pid1 = get_test_pid();
    xinim::pid_t pid2 = get_test_pid();

    CapabilityToken valid_token = create_token(pid1);
    CapabilityToken wrong_pid_token = create_token(pid2);

    // Valid capability should work
    assert(mutex.lock(pid1, valid_token));
    mutex.unlock(pid1);

    // Wrong PID should fail
    assert(!mutex.lock(pid1, wrong_pid_token)); // pid1 using pid2's token

    // Token without LOCK right should fail
    CapabilityToken no_lock_token = create_token(pid1);
    no_lock_token.rights = CapabilityToken::CAP_UNLOCK; // Only unlock right

    assert(!mutex.lock(pid1, no_lock_token));
}

/**
 * @brief Test expired tokens.
 */
static void test_expired_tokens() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();

    CapabilityToken expired_token = create_token(pid);
    expired_token.expiry_time = 1; // Expired (TSC > 1)

    // Expired token should fail
    assert(!mutex.lock(pid, expired_token));
}

/**
 * @brief Test lock manager registration.
 */
static void test_lock_manager_registration() {
    lock_manager.reset_statistics();

    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    assert(lock_manager.lock_count(pid) == 0);

    // Lock - should register
    mutex.lock(pid, token);
    assert(lock_manager.lock_count(pid) == 1);

    // Unlock - should unregister
    mutex.unlock(pid);
    assert(lock_manager.lock_count(pid) == 0);

    auto stats = lock_manager.get_statistics();
    assert(stats.total_acquired == 1);
    assert(stats.total_released == 1);
}

/**
 * @brief Test crash recovery: force unlock.
 */
static void test_crash_recovery() {
    lock_manager.reset_statistics();

    CapabilityMutex mutex;
    xinim::pid_t crashed_pid = get_test_pid();
    CapabilityToken token = create_token(crashed_pid);

    // Process acquires lock
    mutex.lock(crashed_pid, token);
    assert(mutex.is_locked());
    assert(lock_manager.lock_count(crashed_pid) == 1);

    // Process crashes - lock manager handles it
    size_t released = lock_manager.handle_crash(crashed_pid);

    assert(released == 1);
    assert(!mutex.is_locked()); // Lock was force-released
    assert(mutex.is_tainted());  // Lock is marked as tainted
    assert(lock_manager.lock_count(crashed_pid) == 0);

    auto stats = lock_manager.get_statistics();
    assert(stats.total_crashes == 1);
    assert(stats.total_force_released == 1);
}

/**
 * @brief Test multiple locks held by one process.
 */
static void test_multiple_locks_one_process() {
    lock_manager.reset_statistics();

    constexpr int NUM_LOCKS = 5;
    std::vector<CapabilityMutex> mutexes(NUM_LOCKS);

    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    // Acquire all locks
    for (int i = 0; i < NUM_LOCKS; i++) {
        mutexes[i].lock(pid, token);
    }

    assert(lock_manager.lock_count(pid) == NUM_LOCKS);

    // Simulate crash
    size_t released = lock_manager.handle_crash(pid);

    assert(released == NUM_LOCKS);

    // All locks should be force-released
    for (int i = 0; i < NUM_LOCKS; i++) {
        assert(!mutexes[i].is_locked());
        assert(mutexes[i].is_tainted());
    }
}

/**
 * @brief Test tainted flag.
 */
static void test_tainted_flag() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    mutex.lock(pid, token);
    assert(!mutex.is_tainted()); // Fresh lock

    // Force unlock
    mutex.force_unlock(pid);

    assert(mutex.is_tainted()); // Now tainted

    // Clear tainted flag
    mutex.clear_tainted();
    assert(!mutex.is_tainted());
}

/**
 * @brief Test RAII guard.
 */
static void test_raii_guard() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    assert(!mutex.is_locked());

    {
        CapabilityLockGuard guard(mutex, pid, token);
        assert(guard.owns_lock());
        assert(mutex.is_locked());
    } // Guard destructor releases lock

    assert(!mutex.is_locked());
}

/**
 * @brief Test mutual exclusion.
 */
static void test_mutual_exclusion() {
    CapabilityMutex mutex;
    std::atomic<int> counter{0};
    std::atomic<bool> in_critical_section{false};

    constexpr int NUM_THREADS = 8;
    constexpr int ITERATIONS = 100;

    auto worker = [&]() {
        xinim::pid_t pid = get_test_pid();
        CapabilityToken token = create_token(pid);

        for (int i = 0; i < ITERATIONS; i++) {
            if (mutex.lock(pid, token)) {
                // Verify mutual exclusion
                assert(!in_critical_section.load());
                in_critical_section.store(true);

                counter.fetch_add(1, std::memory_order_relaxed);

                in_critical_section.store(false);
                mutex.unlock(pid);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    assert(counter.load() == NUM_THREADS * ITERATIONS);
}

/**
 * @brief Test lock manager statistics.
 */
static void test_lock_manager_statistics() {
    lock_manager.reset_statistics();

    constexpr int NUM_PROCESSES = 5;
    constexpr int LOCKS_PER_PROCESS = 3;

    std::vector<std::vector<CapabilityMutex>> all_mutexes(NUM_PROCESSES);

    for (int i = 0; i < NUM_PROCESSES; i++) {
        all_mutexes[i].resize(LOCKS_PER_PROCESS);

        xinim::pid_t pid = get_test_pid();
        CapabilityToken token = create_token(pid);

        for (int j = 0; j < LOCKS_PER_PROCESS; j++) {
            all_mutexes[i][j].lock(pid, token);
        }
    }

    auto stats = lock_manager.get_statistics();
    assert(stats.active_locks == NUM_PROCESSES * LOCKS_PER_PROCESS);
    assert(stats.active_processes == NUM_PROCESSES);
    assert(stats.total_acquired == NUM_PROCESSES * LOCKS_PER_PROCESS);

    // Unlock half
    for (int i = 0; i < NUM_PROCESSES; i++) {
        xinim::pid_t pid = i + 1; // PIDs start at 1
        all_mutexes[i][0].unlock(pid);
    }

    stats = lock_manager.get_statistics();
    assert(stats.total_released == NUM_PROCESSES);
}

/**
 * @brief Test crash during waiter queue.
 */
static void test_crash_with_waiters() {
    lock_manager.reset_statistics();

    CapabilityMutex mutex;
    xinim::pid_t owner_pid = get_test_pid();
    CapabilityToken owner_token = create_token(owner_pid);

    // Owner acquires lock
    mutex.lock(owner_pid, owner_token);

    // Simulate waiters (in real kernel, they'd block)
    assert(mutex.waiter_count() == 0); // Initial state

    // Owner crashes
    mutex.force_unlock(owner_pid);

    assert(!mutex.is_locked());
    assert(mutex.is_tainted());

    // New process can acquire
    xinim::pid_t new_pid = get_test_pid();
    CapabilityToken new_token = create_token(new_pid);

    assert(mutex.lock(new_pid, new_token));
    mutex.unlock(new_pid);
}

/**
 * @brief Main test runner.
 */
int main() {
    test_basic_capability_lock();
    test_capability_verification();
    test_expired_tokens();
    test_lock_manager_registration();
    test_crash_recovery();
    test_multiple_locks_one_process();
    test_tainted_flag();
    test_raii_guard();
    test_mutual_exclusion();
    test_lock_manager_statistics();
    test_crash_with_waiters();

    return 0;
}

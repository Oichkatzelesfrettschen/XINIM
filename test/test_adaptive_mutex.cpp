/**
 * @file test_adaptive_mutex.cpp
 * @brief Unit tests for AdaptiveMutex.
 */

#include "../src/kernel/adaptive_mutex.hpp"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace xinim::sync;
using namespace std::chrono_literals;

// Mock PID counter for testing
static std::atomic<xinim::pid_t> next_pid{1};

static xinim::pid_t get_test_pid() {
    return next_pid.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Test basic lock/unlock operations.
 */
static void test_basic_lock_unlock() {
    AdaptiveMutex mutex;
    xinim::pid_t pid = get_test_pid();

    // Initially unlocked
    assert(!mutex.is_locked());
    assert(mutex.owner() == 0);

    // Lock and check
    mutex.lock(pid);
    assert(mutex.is_locked());
    assert(mutex.owner() == pid);

    // Unlock and check
    mutex.unlock();
    assert(!mutex.is_locked());
    assert(mutex.owner() == 0);
}

/**
 * @brief Test RAII guard.
 */
static void test_lock_guard() {
    AdaptiveMutex mutex;
    xinim::pid_t pid = get_test_pid();

    assert(!mutex.is_locked());

    {
        AdaptiveLockGuard guard(mutex, pid);
        assert(mutex.is_locked());
        assert(mutex.owner() == pid);
    } // Guard destructor releases lock

    assert(!mutex.is_locked());
}

/**
 * @brief Test try_lock.
 */
static void test_try_lock() {
    AdaptiveMutex mutex;
    xinim::pid_t pid1 = get_test_pid();
    xinim::pid_t pid2 = get_test_pid();

    // Should succeed when unlocked
    assert(mutex.try_lock(pid1));
    assert(mutex.is_locked());
    assert(mutex.owner() == pid1);

    // Should fail when locked
    assert(!mutex.try_lock(pid2));
    assert(mutex.owner() == pid1); // Still owned by pid1

    mutex.unlock();
    assert(!mutex.is_locked());

    // Should succeed again
    assert(mutex.try_lock(pid2));
    assert(mutex.owner() == pid2);
    mutex.unlock();
}

/**
 * @brief Test mutual exclusion with multiple threads.
 */
static void test_mutual_exclusion() {
    AdaptiveMutex mutex;
    std::atomic<int> counter{0};
    std::atomic<bool> in_critical_section{false};

    constexpr int NUM_THREADS = 8;
    constexpr int ITERATIONS = 1000;

    auto worker = [&]() {
        xinim::pid_t pid = get_test_pid();

        for (int i = 0; i < ITERATIONS; i++) {
            mutex.lock(pid);

            // Verify mutual exclusion
            assert(!in_critical_section.load());
            in_critical_section.store(true);

            // Critical section
            counter.fetch_add(1, std::memory_order_relaxed);

            in_critical_section.store(false);
            mutex.unlock();
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
 * @brief Test waiter queue.
 */
static void test_waiter_queue() {
    AdaptiveMutex mutex;
    xinim::pid_t pid1 = get_test_pid();

    assert(mutex.waiter_count() == 0);

    mutex.lock(pid1);
    assert(mutex.waiter_count() == 0); // Owner doesn't count as waiter

    // Simulate waiters (in real kernel, they'd be added during lock())
    // For testing, we just verify the unlock mechanism works

    mutex.unlock();
    assert(mutex.waiter_count() == 0);
}

/**
 * @brief Test fast path performance (uncontended).
 */
static void test_fast_path() {
    AdaptiveMutex mutex;
    xinim::pid_t pid = get_test_pid();

    constexpr int ITERATIONS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        mutex.lock(pid);
        mutex.unlock();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Fast path should be very quick (<1μs per lock/unlock on modern CPU)
    double avg_us = static_cast<double>(duration.count()) / ITERATIONS;

    // Print for manual verification
    // Expected: ~0.01-0.1μs per iteration on modern CPU
    // printf("Average fast-path time: %.3f μs\n", avg_us);

    // Sanity check: should be less than 1μs average
    assert(avg_us < 1.0);
}

/**
 * @brief Test contention handling.
 */
static void test_contention() {
    AdaptiveMutex mutex;
    std::atomic<uint64_t> counter{0};

    constexpr int NUM_THREADS = 16;
    constexpr int ITERATIONS = 1000;

    auto worker = [&]() {
        xinim::pid_t pid = get_test_pid();

        for (int i = 0; i < ITERATIONS; i++) {
            mutex.lock(pid);

            // Hold lock for a bit (simulate work)
            for (volatile int j = 0; j < 100; j++) {
                // Busy work
            }

            counter.fetch_add(1, std::memory_order_relaxed);
            mutex.unlock();
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
 * @brief Test owner tracking.
 */
static void test_owner_tracking() {
    AdaptiveMutex mutex;
    xinim::pid_t pid1 = get_test_pid();
    xinim::pid_t pid2 = get_test_pid();

    assert(mutex.owner() == 0);

    mutex.lock(pid1);
    assert(mutex.owner() == pid1);

    // Another thread tries to lock
    std::thread t([&]() {
        mutex.lock(pid2);
        assert(mutex.owner() == pid2);
        mutex.unlock();
    });

    // Small delay to ensure thread t is blocked
    std::this_thread::sleep_for(10ms);

    // Still owned by pid1
    assert(mutex.owner() == pid1);

    // Release - thread t should acquire
    mutex.unlock();

    t.join();

    // Now unlocked (thread t has released)
    assert(mutex.owner() == 0);
}

/**
 * @brief Main test runner.
 */
int main() {
    test_basic_lock_unlock();
    test_lock_guard();
    test_try_lock();
    test_mutual_exclusion();
    test_waiter_queue();
    test_fast_path();
    test_contention();
    test_owner_tracking();

    return 0;
}

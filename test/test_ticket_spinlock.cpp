/**
 * @file test_ticket_spinlock.cpp
 * @brief Unit tests for TicketSpinlock.
 */

#include "ticket_spinlock.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

using namespace xinim::sync;

/**
 * @brief Test basic lock/unlock operations.
 */
static void test_basic_lock_unlock() {
    TicketSpinlock lock;

    // Initially unlocked
    assert(!lock.is_locked());

    // Lock and check
    lock.lock();
    assert(lock.is_locked());

    // Unlock and check
    lock.unlock();
    assert(!lock.is_locked());
}

/**
 * @brief Test RAII guard.
 */
static void test_lock_guard() {
    TicketSpinlock lock;

    assert(!lock.is_locked());

    {
        TicketLockGuard guard(lock);
        assert(lock.is_locked());
    } // Guard destructor releases lock

    assert(!lock.is_locked());
}

/**
 * @brief Test try_lock.
 */
static void test_try_lock() {
    TicketSpinlock lock;

    // Should succeed when unlocked
    assert(lock.try_lock());
    assert(lock.is_locked());

    // Should fail when locked
    assert(!lock.try_lock());

    lock.unlock();
    assert(!lock.is_locked());

    // Should succeed again
    assert(lock.try_lock());
    lock.unlock();
}

/**
 * @brief Test mutual exclusion with multiple threads.
 */
static void test_mutual_exclusion() {
    TicketSpinlock lock;
    std::atomic<int> counter{0};
    std::atomic<bool> in_critical_section{false};

    constexpr int NUM_THREADS = 8;
    constexpr int ITERATIONS = 1000;

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            lock.lock();

            // Verify mutual exclusion
            assert(!in_critical_section.load());
            in_critical_section.store(true);

            // Critical section
            counter.fetch_add(1, std::memory_order_relaxed);

            in_critical_section.store(false);
            lock.unlock();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(counter.load() == NUM_THREADS * ITERATIONS);
}

/**
 * @brief Test FIFO fairness.
 *
 * Threads should acquire the lock in the order they requested it.
 */
static void test_fifo_fairness() {
    TicketSpinlock lock;
    constexpr std::size_t kThreadCount = 4U;
    std::vector<std::size_t> acquisition_order;
    std::vector<std::size_t> request_order(kThreadCount, 0U);
    std::mutex order_mutex;
    std::atomic<std::size_t> next_request{0U};

    // Pre-acquire the lock to ensure all threads queue up
    lock.lock();

    auto worker = [&](std::size_t thread_index) {
        request_order[thread_index] = next_request.fetch_add(1U, std::memory_order_relaxed);
        lock.lock();

        // Record acquisition order
        {
            std::lock_guard<std::mutex> guard(order_mutex);
            acquisition_order.push_back(request_order[thread_index]);
        }

        lock.unlock();
    };

    std::vector<std::thread> threads;
    for (std::size_t thread_index = 0U; thread_index < kThreadCount; ++thread_index) {
        threads.emplace_back(worker, thread_index);
    }

    // Wait until every thread has requested the lock before releasing it.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (next_request.load(std::memory_order_relaxed) != kThreadCount &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    assert(next_request.load(std::memory_order_relaxed) == kThreadCount);

    // Release the lock - threads should acquire in order
    lock.unlock();

    for (auto &t : threads) {
        t.join();
    }

    // Verify FIFO order by request sequence, not thread creation order.
    assert(acquisition_order.size() == kThreadCount);
    for (std::size_t request_index = 0U; request_index < kThreadCount; ++request_index) {
        assert(acquisition_order[request_index] == request_index);
    }
}

/**
 * @brief Test queue length reporting.
 */
static void test_queue_length() {
    TicketSpinlock lock;

    assert(lock.queue_length() == 0);

    lock.lock();
    assert(lock.queue_length() == 1); // One ticket taken, none served

    lock.unlock();
    assert(lock.queue_length() == 0); // All caught up
}

/**
 * @brief Main test runner.
 */
int main() {
    test_basic_lock_unlock();
    test_lock_guard();
    test_try_lock();
    test_mutual_exclusion();
    test_fifo_fairness();
    test_queue_length();

    return 0;
}

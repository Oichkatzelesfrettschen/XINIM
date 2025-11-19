/**
 * @file test_mcs_spinlock.cpp
 * @brief Unit tests for MCSSpinlock.
 */

#include "../src/kernel/mcs_spinlock.hpp"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

using namespace xinim::sync;

/**
 * @brief Test basic lock/unlock operations.
 */
static void test_basic_lock_unlock() {
    MCSSpinlock lock;
    MCSNode node;

    // Initially unlocked
    assert(!lock.is_locked());

    // Lock and check
    lock.lock(&node);
    assert(lock.is_locked());

    // Unlock and check
    lock.unlock(&node);
    assert(!lock.is_locked());
}

/**
 * @brief Test RAII guard.
 */
static void test_lock_guard() {
    MCSSpinlock lock;

    assert(!lock.is_locked());

    {
        MCSLockGuard guard(lock);
        assert(lock.is_locked());
    } // Guard destructor releases lock

    assert(!lock.is_locked());
}

/**
 * @brief Test try_lock.
 */
static void test_try_lock() {
    MCSSpinlock lock;
    MCSNode node1, node2;

    // Should succeed when unlocked
    assert(lock.try_lock(&node1));
    assert(lock.is_locked());

    // Should fail when locked
    assert(!lock.try_lock(&node2));

    lock.unlock(&node1);
    assert(!lock.is_locked());

    // Should succeed again
    assert(lock.try_lock(&node2));
    lock.unlock(&node2);
}

/**
 * @brief Test mutual exclusion with multiple threads.
 */
static void test_mutual_exclusion() {
    MCSSpinlock lock;
    std::atomic<int> counter{0};
    std::atomic<bool> in_critical_section{false};

    constexpr int NUM_THREADS = 8;
    constexpr int ITERATIONS = 1000;

    auto worker = [&]() {
        MCSNode node; // Per-thread node

        for (int i = 0; i < ITERATIONS; i++) {
            lock.lock(&node);

            // Verify mutual exclusion
            assert(!in_critical_section.load());
            in_critical_section.store(true);

            // Critical section
            counter.fetch_add(1, std::memory_order_relaxed);

            in_critical_section.store(false);
            lock.unlock(&node);
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
 * @brief Test FIFO fairness.
 */
static void test_fifo_fairness() {
    MCSSpinlock lock;
    std::atomic<int> acquisition_order{0};
    std::vector<int> order;
    std::mutex order_mutex;

    constexpr int NUM_THREADS = 4;

    // Pre-acquire the lock to ensure all threads queue up
    MCSNode initial_node;
    lock.lock(&initial_node);

    auto worker = [&](int id) {
        MCSNode node;
        lock.lock(&node);

        // Record acquisition order
        {
            std::lock_guard<std::mutex> guard(order_mutex);
            order.push_back(id);
        }

        lock.unlock(&node);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    // Small delay to ensure all threads have enqueued
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Release the lock - threads should acquire in FIFO order
    lock.unlock(&initial_node);

    for (auto& t : threads) {
        t.join();
    }

    // Verify FIFO order (0, 1, 2, 3)
    assert(order.size() == NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(order[i] == i);
    }
}

/**
 * @brief Test node pool.
 */
static void test_node_pool() {
    MCSNode* node1 = MCSNodePool::acquire_node();
    assert(node1 != nullptr);

    MCSNode* node2 = MCSNodePool::acquire_node();
    assert(node2 != nullptr);

    // In multi-threaded context, each thread should get its own node
    std::vector<std::thread> threads;
    std::vector<MCSNode*> nodes(4);

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&, i]() {
            nodes[i] = MCSNodePool::acquire_node();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All nodes should be unique (thread-local)
    for (int i = 0; i < 4; i++) {
        assert(nodes[i] != nullptr);
        for (int j = i + 1; j < 4; j++) {
            assert(nodes[i] != nodes[j]); // Different thread-local instances
        }
    }
}

/**
 * @brief Stress test: High contention.
 */
static void test_high_contention() {
    MCSSpinlock lock;
    std::atomic<uint64_t> counter{0};

    constexpr int NUM_THREADS = 16;
    constexpr int ITERATIONS = 10000;

    auto worker = [&]() {
        MCSNode node;

        for (int i = 0; i < ITERATIONS; i++) {
            lock.lock(&node);
            counter.fetch_add(1, std::memory_order_relaxed);
            lock.unlock(&node);
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
 * @brief Main test runner.
 */
int main() {
    test_basic_lock_unlock();
    test_lock_guard();
    test_try_lock();
    test_mutual_exclusion();
    test_fifo_fairness();
    test_node_pool();
    test_high_contention();

    return 0;
}

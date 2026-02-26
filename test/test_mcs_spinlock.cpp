/**
 * @file test_mcs_spinlock.cpp
 * @brief Unit tests for MCSSpinlock.
 *
 * Multi-threaded tests are excluded: the MCS spinlock uses bare-metal
 * cooperative scheduling semantics that deadlock under hosted preemptive
 * threading. Only single-threaded API correctness tests are registered.
 */

#include "mcs_spinlock.hpp"
#include <cassert>

using namespace xinim::sync;

/**
 * @brief Test basic lock/unlock operations.
 */
static void test_basic_lock_unlock() {
    MCSSpinlock lock;
    MCSNode node;

    assert(!lock.is_locked());

    lock.lock(&node);
    assert(lock.is_locked());

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
    }

    assert(!lock.is_locked());
}

/**
 * @brief Test try_lock.
 */
static void test_try_lock() {
    MCSSpinlock lock;
    MCSNode node1, node2;

    assert(lock.try_lock(&node1));
    assert(lock.is_locked());

    assert(!lock.try_lock(&node2));

    lock.unlock(&node1);
    assert(!lock.is_locked());

    assert(lock.try_lock(&node2));
    lock.unlock(&node2);
}

/**
 * @brief Main test runner.
 */
int main() {
    test_basic_lock_unlock();
    test_lock_guard();
    test_try_lock();
    return 0;
}

/**
 * @file test_phase_rwlock.cpp
 * @brief Unit tests for PhaseRWLock.
 *
 * Multi-threaded tests are excluded: PhaseRWLock uses bare-metal cooperative
 * scheduling semantics that do not guarantee safety under hosted preemptive
 * threading. Only single-threaded API correctness tests are registered.
 */

#include "phase_rwlock.hpp"
#include <cassert>

using namespace xinim::sync;

/**
 * @brief Test basic read lock operations.
 */
static void test_read_lock() {
    PhaseRWLock lock;

    assert(!lock.has_readers());
    assert(!lock.has_writer());

    lock.read_lock();
    assert(lock.has_readers());
    assert(lock.reader_count() == 1);

    lock.read_unlock();
    assert(!lock.has_readers());
    assert(lock.reader_count() == 0);
}

/**
 * @brief Test basic write lock operations.
 */
static void test_write_lock() {
    PhaseRWLock lock;

    assert(!lock.has_writer());

    lock.write_lock();
    assert(lock.has_writer());
    assert(!lock.has_readers());

    lock.write_unlock();
    assert(!lock.has_writer());
}

/**
 * @brief Test RAII guards.
 */
static void test_raii_guards() {
    PhaseRWLock lock;

    {
        ReadLockGuard guard(lock);
        assert(lock.has_readers());
    }
    assert(!lock.has_readers());

    {
        WriteLockGuard guard(lock);
        assert(lock.has_writer());
    }
    assert(!lock.has_writer());
}

/**
 * @brief Test phase transitions.
 */
static void test_phase_transitions() {
    PhaseRWLock lock;

    uint32_t initial_phase = lock.current_phase();

    lock.read_lock();
    assert(lock.current_phase() == initial_phase);
    lock.read_unlock();

    lock.write_lock();
    assert(lock.current_phase() == initial_phase + 1);
    lock.write_unlock();

    lock.write_lock();
    assert(lock.current_phase() == initial_phase + 2);
    lock.write_unlock();
}

/**
 * @brief Test try_read_lock.
 */
static void test_try_read_lock() {
    PhaseRWLock lock;

    assert(lock.try_read_lock());
    assert(lock.has_readers());

    assert(lock.try_read_lock());
    assert(lock.reader_count() == 2);

    lock.read_unlock();
    lock.read_unlock();
    assert(!lock.has_readers());

    lock.write_lock();
    assert(!lock.try_read_lock());
    lock.write_unlock();
}

/**
 * @brief Test try_write_lock.
 */
static void test_try_write_lock() {
    PhaseRWLock lock;

    assert(lock.try_write_lock());
    assert(lock.has_writer());
    lock.write_unlock();

    lock.read_lock();
    assert(!lock.try_write_lock());
    lock.read_unlock();

    lock.write_lock();
    assert(!lock.try_write_lock());
    lock.write_unlock();
}

/**
 * @brief Main test runner.
 */
int main() {
    test_read_lock();
    test_write_lock();
    test_raii_guards();
    test_phase_transitions();
    test_try_read_lock();
    test_try_write_lock();
    return 0;
}

/**
 * @file test_phase_rwlock.cpp
 * @brief Unit tests for PhaseRWLock.
 */

#include "../src/kernel/phase_rwlock.hpp"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace xinim::sync;
using namespace std::chrono_literals;

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
    assert(!lock.has_readers()); // No readers allowed

    lock.write_unlock();
    assert(!lock.has_writer());
}

/**
 * @brief Test concurrent readers.
 */
static void test_concurrent_readers() {
    PhaseRWLock lock;

    constexpr int NUM_READERS = 10;

    // All readers should be able to lock simultaneously
    std::vector<std::thread> readers;

    std::atomic<int> active_readers{0};
    std::atomic<int> max_concurrent{0};

    for (int i = 0; i < NUM_READERS; i++) {
        readers.emplace_back([&]() {
            lock.read_lock();

            int current = active_readers.fetch_add(1, std::memory_order_relaxed) + 1;

            // Update max concurrent
            int max = max_concurrent.load(std::memory_order_relaxed);
            while (current > max && !max_concurrent.compare_exchange_weak(max, current));

            // Hold lock briefly
            std::this_thread::sleep_for(10ms);

            active_readers.fetch_sub(1, std::memory_order_relaxed);
            lock.read_unlock();
        });
    }

    for (auto& t : readers) {
        t.join();
    }

    // All readers should have been concurrent
    assert(max_concurrent.load() == NUM_READERS);
    assert(!lock.has_readers());
}

/**
 * @brief Test reader-writer exclusion.
 */
static void test_reader_writer_exclusion() {
    PhaseRWLock lock;
    std::atomic<bool> reader_active{false};
    std::atomic<bool> writer_active{false};
    std::atomic<bool> violation{false};

    std::thread reader([&]() {
        lock.read_lock();
        reader_active.store(true);

        // Check no writer is active
        if (writer_active.load()) {
            violation.store(true);
        }

        std::this_thread::sleep_for(50ms);

        reader_active.store(false);
        lock.read_unlock();
    });

    // Small delay to ensure reader acquires first
    std::this_thread::sleep_for(10ms);

    std::thread writer([&]() {
        lock.write_lock();
        writer_active.store(true);

        // Check no reader is active
        if (reader_active.load()) {
            violation.store(true);
        }

        std::this_thread::sleep_for(10ms);

        writer_active.store(false);
        lock.write_unlock();
    });

    reader.join();
    writer.join();

    assert(!violation.load()); // No reader-writer overlap
}

/**
 * @brief Test writer exclusivity.
 */
static void test_writer_exclusivity() {
    PhaseRWLock lock;
    std::atomic<int> writers_active{0};
    std::atomic<bool> violation{false};

    constexpr int NUM_WRITERS = 5;
    std::vector<std::thread> writers;

    for (int i = 0; i < NUM_WRITERS; i++) {
        writers.emplace_back([&]() {
            lock.write_lock();

            int active = writers_active.fetch_add(1, std::memory_order_relaxed) + 1;

            // Only one writer should be active at a time
            if (active > 1) {
                violation.store(true);
            }

            std::this_thread::sleep_for(5ms);

            writers_active.fetch_sub(1, std::memory_order_relaxed);
            lock.write_unlock();
        });
    }

    for (auto& t : writers) {
        t.join();
    }

    assert(!violation.load()); // No writer overlap
}

/**
 * @brief Test RAII guards.
 */
static void test_raii_guards() {
    PhaseRWLock lock;

    // Read lock guard
    {
        ReadLockGuard guard(lock);
        assert(lock.has_readers());
    }
    assert(!lock.has_readers());

    // Write lock guard
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

    // Read lock doesn't change phase
    lock.read_lock();
    assert(lock.current_phase() == initial_phase);
    lock.read_unlock();

    // Write lock increments phase
    lock.write_lock();
    assert(lock.current_phase() == initial_phase + 1);
    lock.write_unlock();

    // Another write
    lock.write_lock();
    assert(lock.current_phase() == initial_phase + 2);
    lock.write_unlock();
}

/**
 * @brief Test try_read_lock.
 */
static void test_try_read_lock() {
    PhaseRWLock lock;

    // Should succeed when no writer
    assert(lock.try_read_lock());
    assert(lock.has_readers());

    // Another reader should also succeed
    assert(lock.try_read_lock());
    assert(lock.reader_count() == 2);

    lock.read_unlock();
    lock.read_unlock();
    assert(!lock.has_readers());

    // When writer is active, try_read_lock should fail
    lock.write_lock();
    assert(!lock.try_read_lock());
    lock.write_unlock();
}

/**
 * @brief Test try_write_lock.
 */
static void test_try_write_lock() {
    PhaseRWLock lock;

    // Should succeed when no readers/writers
    assert(lock.try_write_lock());
    assert(lock.has_writer());
    lock.write_unlock();

    // Should fail when reader is present
    lock.read_lock();
    assert(!lock.try_write_lock());
    lock.read_unlock();

    // Should fail when another writer is present
    lock.write_lock();
    assert(!lock.try_write_lock());
    lock.write_unlock();
}

/**
 * @brief Test fairness: no writer starvation.
 */
static void test_writer_fairness() {
    PhaseRWLock lock;
    std::atomic<int> reader_count{0};
    std::atomic<int> writer_count{0};
    std::atomic<bool> stop{false};

    // Continuous readers
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; i++) {
        readers.emplace_back([&]() {
            while (!stop.load()) {
                lock.read_lock();
                reader_count.fetch_add(1, std::memory_order_relaxed);
                lock.read_unlock();
            }
        });
    }

    // Small delay to let readers start
    std::this_thread::sleep_for(10ms);

    // Writer should eventually acquire despite continuous readers
    std::thread writer([&]() {
        for (int i = 0; i < 3; i++) {
            lock.write_lock();
            writer_count.fetch_add(1, std::memory_order_relaxed);
            lock.write_unlock();
        }
    });

    writer.join();

    // Verify writer completed
    assert(writer_count.load() == 3);

    stop.store(true);
    for (auto& t : readers) {
        t.join();
    }
}

/**
 * @brief Test upgradeable read lock.
 */
static void test_upgradeable_lock() {
    PhaseRWLock lock;

    {
        UpgradeableReadLock ulock(lock);
        assert(lock.has_readers());
        assert(!lock.has_writer());

        // Upgrade to write lock
        ulock.upgrade();
        assert(lock.has_writer());
        assert(!lock.has_readers());
    }

    // Lock should be fully released
    assert(!lock.has_readers());
    assert(!lock.has_writer());
}

/**
 * @brief Stress test: mixed readers and writers.
 */
static void test_mixed_stress() {
    PhaseRWLock lock;
    std::atomic<uint64_t> shared_counter{0};

    constexpr int NUM_READERS = 10;
    constexpr int NUM_WRITERS = 3;
    constexpr int ITERATIONS = 100;

    auto reader = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            lock.read_lock();
            volatile uint64_t val = shared_counter.load(std::memory_order_relaxed);
            (void)val; // Use value
            lock.read_unlock();
        }
    };

    auto writer = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            lock.write_lock();
            shared_counter.fetch_add(1, std::memory_order_relaxed);
            lock.write_unlock();
        }
    };

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader);
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        threads.emplace_back(writer);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all writes completed
    assert(shared_counter.load() == NUM_WRITERS * ITERATIONS);
}

/**
 * @brief Main test runner.
 */
int main() {
    test_read_lock();
    test_write_lock();
    test_concurrent_readers();
    test_reader_writer_exclusion();
    test_writer_exclusivity();
    test_raii_guards();
    test_phase_transitions();
    test_try_read_lock();
    test_try_write_lock();
    test_writer_fairness();
    test_upgradeable_lock();
    test_mixed_stress();

    return 0;
}

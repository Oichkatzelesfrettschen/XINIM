/**
 * @file test_scheduler_edge.cpp
 * @brief Edge case unit tests for the Scheduler class.
 */

#include "../kernel/schedule.hpp"
#include <cassert>

/**
 * @brief Validate scheduler behavior with empty queues and missing targets.
 *
 * This test exercises preemption with an empty queue and the yield_to logic
 * when the target thread is not present.
 */

/**
 * @brief Entry point for scheduler edge case tests.
 *
 * The function verifies preemption semantics when the run queue is empty and
 * tests @c Scheduler::yield_to when the target thread is absent. A successful
 * run returns @c 0.
 *
 * @return Exit status code.
 */
int main() {
    using sched::scheduler;

    // Preempt with no ready threads should return nullopt.
    assert(!scheduler.preempt().has_value());

    // Enqueue a single thread and preempt to schedule it.
    scheduler.enqueue(10);
    auto first = scheduler.preempt();
    assert(first && *first == 10);

    // The queue is now empty; another preempt should fail.
    assert(!scheduler.preempt().has_value());

    // Add two threads and switch to the first.
    scheduler.enqueue(11);
    scheduler.enqueue(12);
    scheduler.preempt(); // now running thread 11

    // Yielding to a nonexistent thread should not change the current thread.
    scheduler.yield_to(42);
    assert(scheduler.current() == 11);

    // Switch to a queued thread explicitly.
    scheduler.enqueue(13);
    scheduler.yield_to(13);
    assert(scheduler.current() == 13);

    return 0;
}

/**
 * @file test_scheduler_deadlock.cpp
 * @brief Unit tests covering Scheduler deadlock detection.
 */

#include "../kernel/schedule.hpp"
#include <cassert>

/**
 * @brief Validate wait-for graph cycle detection in Scheduler.
 */
int main() {
    using sched::scheduler;

    scheduler.enqueue(1);
    scheduler.enqueue(2);

    // Run the first thread.
    auto cur = scheduler.preempt();
    assert(cur && *cur == 1);

    // Block thread 1 waiting for thread 2. This should succeed.
    assert(scheduler.block_on(1, 2));

    // Scheduler should now run thread 2.
    cur = scheduler.preempt();
    assert(cur && *cur == 2);

    // Attempting to block thread 2 on thread 1 would create a cycle.
    assert(!scheduler.block_on(2, 1));
    assert(scheduler.current() == 2);

    // Unblock thread 1 and ensure it can run again.
    scheduler.unblock(1);
    scheduler.yield_to(1);
    assert(scheduler.current() == 1);

    return 0;
}

/**
 * @file test_scheduler.cpp
 * @brief Unit tests for sched::Scheduler round-robin scheduling.
 */

#include "schedule.hpp"
#include <cassert>

int main() {
    // Use a fresh scheduler instance (not the global)
    sched::Scheduler s;

    // Empty scheduler returns -1
    assert(s.pick_next() == -1);
    assert(s.current() == -1);

    // Enqueue two processes
    s.ready(1);
    s.ready(2);

    // pick_next returns them in FIFO order
    auto first = s.pick_next();
    assert(first == 1);
    assert(s.current() == 1);

    auto second = s.pick_next();
    assert(second == 2);
    assert(s.current() == 2);

    // Queue is now empty
    assert(s.pick_next() == -1);

    // yield_to sets a specific process as current
    s.ready(3);
    s.ready(4);
    s.yield_to(4);
    assert(s.current() == 4);

    // Process 3 should still be in the ready queue
    auto next = s.pick_next();
    assert(next == 3);

    // Test block_on and unblock
    s.ready(10);
    s.ready(11);
    s.block_on(11, 10);
    // Only 10 should be ready now
    assert(s.pick_next() == 10);
    assert(s.pick_next() == -1); // 11 is blocked
    s.unblock(11);
    assert(s.pick_next() == 11); // now unblocked

    // Duplicate ready calls are idempotent
    s.ready(20);
    s.ready(20);
    assert(s.pick_next() == 20);
    assert(s.pick_next() == -1); // only one instance

    return 0;
}

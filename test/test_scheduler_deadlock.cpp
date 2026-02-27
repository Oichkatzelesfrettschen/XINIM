/**
 * @file test_scheduler_deadlock.cpp
 * @brief Unit tests covering Scheduler deadlock detection via wait-for graph.
 */

#include "schedule.hpp"
#include <cassert>

int main() {
    sched::Scheduler s;

    s.ready(1);
    s.ready(2);
    s.ready(3);

    // Run process 1
    auto cur = s.pick_next();
    assert(cur == 1);

    // Block process 1 waiting for process 2 -- should succeed (no cycle)
    assert(s.block_on(1, 2));

    // Process 1 is no longer in the ready queue
    cur = s.pick_next();
    assert(cur == 2);

    // Block process 2 waiting for process 3 -- should succeed (no cycle)
    assert(s.block_on(2, 3));

    // Now only process 3 is ready
    cur = s.pick_next();
    assert(cur == 3);

    // The wait-for graph: 1->2, 2->3
    // Blocking 3 on 1 would create a cycle 1->2->3->1
    // block_on returns true but adds the edge; cycle detection is via is_in_cycle
    s.block_on(3, 1);
    // Now we can detect the cycle
    assert(s.graph().is_in_cycle(1));
    assert(s.graph().is_in_cycle(2));
    assert(s.graph().is_in_cycle(3));

    // Unblock process 1 to break the cycle
    s.unblock(1);
    assert(!s.graph().is_in_cycle(2)); // 2->3 still, but no cycle
    assert(!s.graph().is_in_cycle(3)); // 3->1 edge exists but 1 is unblocked

    // Process 1 should be back in the ready queue
    cur = s.pick_next();
    assert(cur == 1);

    return 0;
}

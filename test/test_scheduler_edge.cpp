/**
 * @file test_scheduler_edge.cpp
 * @brief Edge case unit tests for the Scheduler class.
 */

#include "schedule.hpp"
#include <cassert>

int main() {
    sched::Scheduler s;

    // pick_next with empty queue returns -1
    assert(s.pick_next() == -1);

    // Ready a single process, pick_next should return it
    s.ready(10);
    auto first = s.pick_next();
    assert(first == 10);

    // Queue is now empty again
    assert(s.pick_next() == -1);

    // yield_to a process not in the ready queue
    s.ready(11);
    s.ready(12);
    s.yield_to(12); // should pull 12 out and set as current
    assert(s.current() == 12);

    // 11 should still be in the ready queue
    auto next = s.pick_next();
    assert(next == 11);

    // unready a process that was never readied is safe
    s.unready(999);

    // Ready and unready the same process
    s.ready(20);
    s.unready(20);
    assert(s.pick_next() == -1);

    // Rapid ready/pick_next cycles
    for (int i = 0; i < 50; ++i) {
        s.ready(static_cast<xinim::pid_t>(i));
    }
    for (int i = 0; i < 50; ++i) {
        auto pid = s.pick_next();
        assert(pid == static_cast<xinim::pid_t>(i));
    }
    assert(s.pick_next() == -1);

    return 0;
}

#include "../kernel/schedule.hpp"
#include <cassert>

int main() {
    using sched::scheduler;
    scheduler.enqueue(1);
    scheduler.enqueue(2);
    auto first = scheduler.preempt();
    assert(first && *first == 1);
    auto second = scheduler.preempt();
    assert(second && *second == 2);
    scheduler.yield_to(1);
    assert(scheduler.current() == 1);
    return 0;
}

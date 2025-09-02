/**
 * @file test_service_manager_dag.cpp
 * @brief Validate ServiceManager dependency cycles and restart ordering.
 */

#include "../kernel/schedule.hpp"
#include "../kernel/service.hpp"
#include <cassert>

/**
 * @brief Exercise dependency management and crash handling.
 */
int main() {
    using sched::scheduler;
    using svc::service_manager;

    service_manager.register_service(1);
    service_manager.register_service(2, {1});
    service_manager.register_service(3, {2});

    // Remove queued entries to start with a clean scheduler state.
    scheduler = sched::Scheduler{};

    // Adding 1 -> 3 would introduce a cycle and must be ignored.
    service_manager.add_dependency(1, 3);

    // Crash service 3 and ensure only it restarts.
    scheduler.crash(3);
    auto next = scheduler.preempt();
    assert(next && *next == 3);
    assert(service_manager.contract(1).restarts == 0);
    assert(service_manager.contract(2).restarts == 0);
    assert(service_manager.contract(3).restarts == 1);

    // Prepare a clean run queue for the ordering test.
    scheduler = sched::Scheduler{};

    // Crashing service 1 must restart dependents in topological order.
    scheduler.crash(1);
    next = scheduler.preempt();
    assert(next && *next == 1);
    next = scheduler.preempt();
    assert(next && *next == 2);
    next = scheduler.preempt();
    assert(next && *next == 3);

    // Verify cumulative restart counters.
    assert(service_manager.contract(1).restarts == 1);
    assert(service_manager.contract(2).restarts == 1);
    assert(service_manager.contract(3).restarts == 2);

    return 0;
}

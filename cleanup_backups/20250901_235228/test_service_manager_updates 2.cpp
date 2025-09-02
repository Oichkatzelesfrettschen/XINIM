/**
 * @file test_service_manager_updates.cpp
 * @brief Unit tests for service deregistration and dependency removal.
 */

#include "../kernel/schedule.hpp"
#include "../kernel/service.hpp"
#include <cassert>

int main() {
    using sched::scheduler;
    using svc::service_manager;

    // Verify that unregistered services disappear from the manager.
    service_manager.register_service(42);
    service_manager.unregister_service(42);
    assert(!service_manager.is_running(42));
    assert(service_manager.contract(42).id == 0);
    assert(!service_manager.handle_crash(42));

    // Verify dependency removal stops chained restarts.
    service_manager.register_service(1);
    service_manager.register_service(2, {1});

    scheduler.preempt();
    scheduler.preempt();

    service_manager.remove_dependency(2, 1);

    scheduler.crash(1);
    scheduler.preempt();
    assert(service_manager.contract(2).restarts == 0);

    return 0;
}

/**
 * @file test_service_contract.cpp
 * @brief Unit tests exercising service restart contracts.
 */

#include "../kernel/net_driver.hpp"
#include "../kernel/schedule.hpp"
#include "../kernel/service.hpp"
#include <cassert>

/**
 * @brief Validate restart limits during scheduler crashes.
 */
int main() {
    using sched::scheduler;
    using svc::service_manager;

    service_manager.register_service(1, {}, 1, net::local_node());

    scheduler.preempt();
    scheduler.crash(1);

    scheduler.preempt(); // run restarted service
    scheduler.crash(1);  // exceeds restart limit

    auto next = scheduler.preempt(); // drain leftover
    next = scheduler.preempt();
    assert(!next);
    assert(service_manager.contract(1).restarts == 1);
    assert(!service_manager.is_running(1));

    return 0;
}

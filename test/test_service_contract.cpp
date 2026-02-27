/**
 * @file test_service_contract.cpp
 * @brief Unit tests exercising service restart contracts.
 */

#include "schedule.hpp"
#include "service.hpp"
#include <cassert>

int main() {
    // Use a fresh scheduler and service manager (not globals)
    sched::Scheduler sched_inst;
    svc::ServiceManager svc_mgr;

    // Register a service with PID 1
    svc_mgr.register_service(1, "test_svc");
    auto* info = svc_mgr.get_service(1);
    assert(info != nullptr);
    assert(info->active);
    assert(info->restart_count == 0);

    // Crash the service -- should be allowed to restart (limit is 5)
    assert(svc_mgr.handle_crash(1));
    assert(info->restart_count == 1);
    assert(info->active);

    // Crash 4 more times to reach the limit
    assert(svc_mgr.handle_crash(1)); // 2
    assert(svc_mgr.handle_crash(1)); // 3
    assert(svc_mgr.handle_crash(1)); // 4
    assert(svc_mgr.handle_crash(1)); // 5

    // 6th crash exceeds limit -- service should be deactivated
    assert(!svc_mgr.handle_crash(1));
    assert(!info->active);
    assert(info->restart_count == 5);

    // Unknown PID returns false
    assert(!svc_mgr.handle_crash(99));

    // Scheduler handle_crash integration
    sched_inst.ready(2);
    svc_mgr.register_service(2, "svc2");
    // handle_crash on scheduler delegates to service_manager
    // (uses global service_manager, so test the standalone svc_mgr separately)

    return 0;
}

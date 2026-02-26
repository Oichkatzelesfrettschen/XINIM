/**
 * @file test_service_manager_api.cpp
 * @brief Unit tests for svc::ServiceManager against the current API.
 *
 * WHY: The original test_service_manager_dag/updates/serialization files target
 *      an older, richer API (add_dependency, unregister_service, contract) that
 *      no longer exists. These tests verify the current bare-metal ServiceManager.
 */

#include "../kernel/service.hpp"
#include <cassert>
#include <cstring>

using svc::ServiceManager;
using svc::ServiceInfo;

static void test_register_and_get() {
    ServiceManager mgr;
    mgr.register_service(42, "test_svc");
    ServiceInfo* info = mgr.get_service(42);
    assert(info != nullptr);
    assert(info->pid == 42);
    assert(std::string_view(info->name) == "test_svc");
    assert(info->active);
    assert(info->restart_count == 0);
}

static void test_get_nonexistent() {
    ServiceManager mgr;
    ServiceInfo* info = mgr.get_service(999);
    assert(info == nullptr);
}

static void test_handle_crash_increments_restart() {
    ServiceManager mgr;
    mgr.register_service(1, "crashable");
    bool allowed = mgr.handle_crash(1);
    assert(allowed); // restart allowed on first crash
    ServiceInfo* info = mgr.get_service(1);
    assert(info != nullptr);
    assert(info->restart_count == 1);
}

static void test_handle_crash_nonexistent() {
    ServiceManager mgr;
    bool result = mgr.handle_crash(999);
    assert(!result); // unknown PID, no restart
}

static void test_multiple_services() {
    ServiceManager mgr;
    mgr.register_service(10, "svc_a");
    mgr.register_service(20, "svc_b");
    mgr.register_service(30, "svc_c");

    assert(mgr.get_service(10) != nullptr);
    assert(mgr.get_service(20) != nullptr);
    assert(mgr.get_service(30) != nullptr);
    assert(mgr.get_service(40) == nullptr);
}

int main() {
    test_register_and_get();
    test_get_nonexistent();
    test_handle_crash_increments_restart();
    test_handle_crash_nonexistent();
    test_multiple_services();
    return 0;
}

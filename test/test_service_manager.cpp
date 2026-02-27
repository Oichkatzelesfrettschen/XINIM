/**
 * @file test_service_manager.cpp
 * @brief Standalone unit tests for svc::ServiceManager.
 *
 * Tests registration, crash handling, restart limits, and capacity.
 */

#include "service.hpp"
#include <cassert>
#include <cstring>

using svc::ServiceManager;

static void test_register_and_lookup() {
    ServiceManager sm;

    sm.register_service(1, "init");
    sm.register_service(2, "vfs");
    sm.register_service(3, "pm");

    auto* s1 = sm.get_service(1);
    auto* s2 = sm.get_service(2);
    auto* s3 = sm.get_service(3);

    assert(s1 != nullptr);
    assert(s2 != nullptr);
    assert(s3 != nullptr);

    assert(s1->pid == 1);
    assert(s1->active);
    assert(s1->restart_count == 0);
    assert(std::strcmp(s1->name, "init") == 0);

    assert(s2->pid == 2);
    assert(std::strcmp(s2->name, "vfs") == 0);
}

static void test_lookup_nonexistent() {
    ServiceManager sm;
    sm.register_service(1, "init");

    assert(sm.get_service(2) == nullptr);
    assert(sm.get_service(-1) == nullptr);
    assert(sm.get_service(999) == nullptr);
}

static void test_crash_restart_cycle() {
    ServiceManager sm;
    sm.register_service(10, "test_svc");

    auto* info = sm.get_service(10);
    assert(info != nullptr);

    // First 4 crashes: restart allowed (returns true)
    for (int i = 0; i < 4; ++i) {
        bool restarted = sm.handle_crash(10);
        assert(restarted);
        assert(info->restart_count == i + 1);
        assert(info->active);
    }

    // 5th crash: restart allowed (limit is 5 restarts)
    bool restarted = sm.handle_crash(10);
    assert(restarted);
    assert(info->restart_count == 5);
    assert(info->active);

    // 6th crash: exceeds limit, service deactivated
    restarted = sm.handle_crash(10);
    assert(!restarted);
    assert(!info->active);
    assert(info->restart_count == 5); // count doesn't increase past limit
}

static void test_crash_nonexistent_pid() {
    ServiceManager sm;
    // Crashing a PID that doesn't exist should return false
    assert(!sm.handle_crash(999));
}

static void test_name_truncation() {
    ServiceManager sm;
    // ServiceInfo.name is char[32], so long names should be truncated
    sm.register_service(1, "a_very_long_service_name_that_exceeds_the_buffer_size");

    auto* info = sm.get_service(1);
    assert(info != nullptr);
    assert(info->active);
    // Name should be null-terminated within the buffer
    assert(info->name[31] == '\0');
}

static void test_multiple_services_independent() {
    ServiceManager sm;
    sm.register_service(1, "svc_a");
    sm.register_service(2, "svc_b");

    // Crash svc_a multiple times -- should not affect svc_b
    for (int i = 0; i < 5; ++i) {
        sm.handle_crash(1);
    }

    auto* a = sm.get_service(1);
    auto* b = sm.get_service(2);
    assert(a->restart_count == 5);
    assert(b->restart_count == 0);
    assert(b->active);
}

static void test_capacity() {
    ServiceManager sm;

    // Register up to MAX_SERVICES
    for (int i = 0; i < ServiceManager::MAX_SERVICES; ++i) {
        sm.register_service(static_cast<xinim::pid_t>(i + 1), "svc");
    }

    // All should be findable
    for (int i = 0; i < ServiceManager::MAX_SERVICES; ++i) {
        assert(sm.get_service(static_cast<xinim::pid_t>(i + 1)) != nullptr);
    }

    // Beyond capacity: registration should silently fail (no crash)
    sm.register_service(999, "overflow");
    // The overflowed service should not be findable
    assert(sm.get_service(999) == nullptr);
}

int main() {
    test_register_and_lookup();
    test_lookup_nonexistent();
    test_crash_restart_cycle();
    test_crash_nonexistent_pid();
    test_name_truncation();
    test_multiple_services_independent();
    test_capacity();
    return 0;
}

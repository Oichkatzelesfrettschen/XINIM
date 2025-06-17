/**
 * @file test_service_serialization.cpp
 * @brief Verify that service state persists and reloads correctly.
 */

#include "../kernel/service.hpp"
#include <cassert>
#include <filesystem>

/**
 * @brief Entry point verifying persistence of service manager state.
 */
int main() {
    using svc::ServiceManager;
    const std::string path{"services_test.json"};

    ServiceManager mgr;
    mgr.register_service(1, {}, 2);
    mgr.register_service(2, {1}, 1);
    mgr.save(path);

    ServiceManager loaded;
    loaded.load(path);

    assert(loaded.contract(1).policy.limit == 2);
    assert(loaded.contract(2).policy.limit == 1);

    // Crash service 1 and ensure dependent service 2 also restarts
    loaded.handle_crash(1);
    assert(loaded.contract(1).restarts == 1);
    assert(loaded.contract(2).restarts == 1);

    std::filesystem::remove(path);
    return 0;
}

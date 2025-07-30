/**
 * @file test_service_serialization.cpp
 * @brief Verify that service state persists and reloads correctly.
 */

#include "../kernel/service.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>

/**
 * @brief Verify persistence operations of the service manager.
 *
 * The test writes a service
 * configuration to disk, exercises
 * both error paths and successful reloading, then validates
 *
 * that restart policies and counters survive the round trip.
 *
 * @return 0 on success, non-zero
 * otherwise.
 */
int main() {
    using svc::ServiceManager;
    const std::string path{"services_test.json"};

    ServiceManager mgr;
    mgr.register_service(1, {}, 2);
    mgr.register_service(2, {1}, 1);
    mgr.save(path);

    // Test loading from a non-existent file
    {
        ServiceManager mgr2;
        const std::string missing_path{"nonexistent_services.json"};
        mgr2.load(missing_path);
        // Expectation: loading a non-existent file leaves the manager empty
        assert(!mgr2.is_running(1));
    }

    // Test loading from a malformed file
    {
        const std::string malformed_path{"malformed_services.json"};
        // Write malformed JSON to file
        std::ofstream ofs(malformed_path);
        ofs << "{ this is not valid JSON! ";
        ofs.close();

        ServiceManager mgr3;
        mgr3.load(malformed_path);
        // Manager should remain empty on failure
        assert(!mgr3.is_running(1));

        // Clean up
        std::filesystem::remove(malformed_path);
    }
    ServiceManager loaded;
    loaded.load(path);

    assert(loaded.contract(1).policy.limit == 2);
    assert(loaded.contract(2).policy.limit == 1);

    // Assert contract.id is correct
    assert(loaded.contract(1).id == 1);
    assert(loaded.contract(2).id == 2);

    // Assert contract.restarts is zero after load
    assert(loaded.contract(1).restarts == 0);
    assert(loaded.contract(2).restarts == 0);

    // Crash service 1 and ensure dependent service 2 also restarts
    (void)loaded.handle_crash(1);
    assert(loaded.contract(1).restarts == 1);
    assert(loaded.contract(2).restarts == 1);

    std::filesystem::remove(path);
    return 0;
}

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

    // Test loading from a non-existent file
    {
        ServiceManager mgr2;
        const std::string missing_path{"nonexistent_services.json"};
        bool load_result = mgr2.load(missing_path);
        assert(!load_result && "Loading from a non-existent file should fail");
    }

    // Test loading from a malformed file
    {
        const std::string malformed_path{"malformed_services.json"};
        // Write malformed JSON to file
        std::ofstream ofs(malformed_path);
        ofs << "{ this is not valid JSON! ";
        ofs.close();

        ServiceManager mgr3;
        bool load_result = mgr3.load(malformed_path);
        assert(!load_result && "Loading from a malformed file should fail");

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

    // Assert deps are correct
    assert(loaded.contract(1).deps.empty());
    assert(loaded.contract(2).deps.size() == 1);
    assert(loaded.contract(2).deps[0] == 1);

    // Crash service 1 and ensure dependent service 2 also restarts
    loaded.handle_crash(1);
    assert(loaded.contract(1).restarts == 1);
    assert(loaded.contract(2).restarts == 1);

    std::filesystem::remove(path);
    return 0;
}

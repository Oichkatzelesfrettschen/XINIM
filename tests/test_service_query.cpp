/**
 * @file test_service_query.cpp
 * @brief Unit tests for ServiceManager enumeration helpers.
 */

#include "../kernel/service.hpp"
#include <algorithm>
#include <cassert>
#include <ranges>

/**
 * @brief Ensure service enumeration and dependency queries work.
 */
int main() {
    using svc::service_manager;

    service_manager.register_service(1);
    service_manager.register_service(2, {1});
    service_manager.register_service(3, {1, 2});

    auto ids = service_manager.list_services();
    assert(ids.size() == 3);
    assert(std::ranges::contains(ids, 1));
    assert(std::ranges::contains(ids, 2));
    assert(std::ranges::contains(ids, 3));

    auto deps2 = service_manager.dependencies(2);
    assert(deps2.size() == 1 && deps2[0] == 1);

    auto deps3 = service_manager.dependencies(3);
    assert(deps3.size() == 2);
    assert(std::ranges::contains(deps3, 1));
    assert(std::ranges::contains(deps3, 2));

    return 0;
}

/**
 * @file test_net_driver_persistent_id.cpp
 * @brief Ensure local node identifier persists across driver runs.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <filesystem>
#include <unistd.h>

int main() {
    std::filesystem::path dir{"/tmp/xinim_persist"};
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto file = dir / "node_id";
    ::unlink(file.c_str());

    net::init(net::Config{0, 16000, 0, net::OverflowPolicy::DropNewest, dir});
    const auto first = net::local_node();
    assert(first != 0);
    net::shutdown();

    net::init(net::Config{0, 16000, 0, net::OverflowPolicy::DropNewest, dir});
    const auto second = net::local_node();
    assert(first == second);
    net::shutdown();

    std::filesystem::remove_all(dir);
    return 0;
}
/**
 * @file test_net_driver_persistent_id.cpp
 * @brief Ensure local node identifier persists across driver runs.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <unistd.h>

int main() {
    ::unlink("/etc/xinim/node_id");

    net::driver.init(net::Config{0, 16000});
    const auto first = net::driver.local_node();
    assert(first != 0);
    net::driver.shutdown();

    net::driver.init(net::Config{0, 16000});
    const auto second = net::driver.local_node();
    assert(first == second);
    net::driver.shutdown();

    ::unlink("/etc/xinim/node_id");
    return 0;
}

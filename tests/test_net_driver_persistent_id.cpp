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

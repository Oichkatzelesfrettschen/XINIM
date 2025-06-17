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

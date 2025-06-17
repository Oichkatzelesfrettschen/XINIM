/**
 * @file test_net_driver_persistent_id.cpp
 * @brief Ensure local node identifier persists across driver runs.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <unistd.h>

int main() {
    ::unlink("/etc/xinim/node_id");

    net::init(net::Config{0, 16000});
    const auto first = net::local_node();
    assert(first != 0);
    net::shutdown();

    net::init(net::Config{0, 16000});
    const auto second = net::local_node();
    assert(first == second);
    net::shutdown();

    ::unlink("/etc/xinim/node_id");
    return 0;
}

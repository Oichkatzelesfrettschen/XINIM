/**
 * @file test_net_driver_persistent_id.cpp
 * @brief Ensure local node identifier persists across driver runs.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <unistd.h>

int main() {
    static constexpr char NODE_ID_FILE[] = "/tmp/xinim_node_id";
    ::unlink(NODE_ID_FILE);

    net::init(net::Config{0, 16000, 0, net::OverflowPolicy::DropNewest, NODE_ID_FILE});
    const auto first = net::local_node();
    assert(first != 0);
    net::shutdown();

    net::init(net::Config{0, 16000, 0, net::OverflowPolicy::DropNewest, NODE_ID_FILE});
    const auto second = net::local_node();
    assert(first == second);
    net::shutdown();

    ::unlink(NODE_ID_FILE);
    return 0;
}

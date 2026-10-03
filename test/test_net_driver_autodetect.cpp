/**
 * @file test_net_driver_autodetect.cpp
 * @brief Verify automatic node ID detection when none is provided.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>

int main() {
    constexpr std::uint16_t PORT = 14999;
    net::init({0, PORT});
    assert(net::local_node() != 0);
    net::shutdown();
    return 0;
}

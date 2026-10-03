/**
 * @file test_net_driver_socket_failure.cpp
 * @brief Verify graceful handling of socket failures in the network driver.
 */

#include "../kernel/net_driver.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
    constexpr net::node_t SELF = 200;
    constexpr net::node_t PEER = 201;
    constexpr uint16_t PORT_SELF = 17050;
    constexpr uint16_t PORT_PEER = 17051;

    net::init(net::Config{SELF, PORT_SELF});
    net::add_remote(PEER, "127.0.0.1", PORT_PEER);

    net::simulate_socket_failure();
    std::this_thread::sleep_for(50ms);

    std::array<std::byte, 1> payload{std::byte{0}};
    assert(net::send(PEER, payload) == std::errc::io_error);

    net::shutdown();
    return 0;
}

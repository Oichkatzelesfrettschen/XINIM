/**
 * @file test_net_driver_loopback.cpp
 * @brief Validate sending a packet to ourselves over UDP loopback.
 */

#include "../kernel/net_driver.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
    constexpr net::node_t SELF = 42;
    constexpr uint16_t PORT = 16050;

    net::Config cfg{SELF, PORT};
    net::driver.init(cfg);
    net::driver.add_remote(SELF, "127.0.0.1", PORT);

    std::array<std::byte, 2> payload{std::byte{0xAA}, std::byte{0x55}};
    assert(net::driver.send(SELF, payload) == std::errc{});

    net::Packet pkt{};
    for (int i = 0; i < 100 && !net::driver.recv(pkt); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == SELF);
    assert(pkt.payload.size() == payload.size());
    assert(pkt.payload[0] == payload[0]);
    assert(pkt.payload[1] == payload[1]);

    net::driver.shutdown();
    return 0;
}

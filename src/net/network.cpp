/**
 * @file network.cpp
 * @brief Network Stack Implementation for Lattice IPC
 */

#include <xinim/net/network.hpp>
#include "console.hpp"
#include <vector>
#include <span>
#include <system_error>

namespace net {

bool initialize() {
    Console::printf("Network stack initialized.\n");
    return true;
}

void shutdown() noexcept {
}

node_t local_node() noexcept {
    return 1; 
}

std::errc send(node_t node, std::span<const std::byte> data) {
    (void)node; (void)data;
    return std::errc{};
}

bool recv(Packet &out) {
    (void)out;
    return false;
}

} // namespace net


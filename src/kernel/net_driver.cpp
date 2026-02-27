/**
 * @file net_driver.cpp
 * @brief Stub network driver for freestanding kernel (no POSIX sockets).
 *
 * Replaces the userland socket-based implementation. All operations return
 * failure until a real hardware driver (E1000/virtio-net) is implemented
 * in Phase 6.
 */

#include "xinim/net/net_driver.hpp"

namespace net {

bool NetDriver::init([[maybe_unused]] const Config& cfg) noexcept {
    return false; // No hardware driver yet
}

bool NetDriver::send([[maybe_unused]] node_t node,
                     [[maybe_unused]] std::span<const std::byte> data) noexcept {
    return false; // No hardware driver yet
}

bool NetDriver::recv([[maybe_unused]] Packet& out) noexcept {
    return false; // No hardware driver yet
}

void NetDriver::shutdown() noexcept {
    // Nothing to clean up
}

node_t local_node() noexcept {
    return 1;
}

} // namespace net

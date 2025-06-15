#include "net_driver.hpp"

/**
 * @file net_driver.cpp
 * @brief Stub network driver for lattice IPC.
 */

namespace net {

int local_node() noexcept { return 0; }

void send(int /*node*/, std::span<const std::byte> /*data*/) {
    // Placeholder: real driver would transmit bytes
}

} // namespace net

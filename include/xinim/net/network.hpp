/**
 * @file network.hpp
 * @brief Network Stack Interface for Lattice IPC
 */

#ifndef XINIM_NET_NETWORK_HPP
#define XINIM_NET_NETWORK_HPP

#include <cstddef>
#include <vector>
#include <span>
#include <system_error>
#include <xinim/net/net_driver.hpp>

namespace net {
    using node_t = int;
    bool initialize();
    node_t local_node() noexcept;
    std::errc send(node_t node, std::span<const std::byte> data);
    bool recv(Packet &out);
} // namespace net

namespace xinim::net {
    using namespace ::net;
}

#endif // XINIM_NET_NETWORK_HPP

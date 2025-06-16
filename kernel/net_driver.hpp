#pragma once
/**
 * @file net_driver.hpp
 * @brief Minimal network driver interface used by lattice IPC.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace net {

/** \brief Integer type representing a network node identifier. */
using node_t = int;

/**
 * @brief Obtain the identifier for the local node.
 *
 * @return Node id of the current host.
 */
[[nodiscard]] int local_node() noexcept;

/**
 * @brief Send raw bytes to @p node.
 *
 * Implementations may queue or transmit the bytes immediately.
 *
 * @param node Destination node identifier.
 * @param data Bytes to transmit.
 */
void send(int node, std::span<const std::byte> data);

/**
 * @brief In-memory representation of a network packet.
 */
struct Packet {
    node_t src_node;                ///< Identifier of the originating node
    std::vector<std::byte> payload; ///< Raw bytes transmitted over the network
};

/**
 * @brief Retrieve a pending packet for the local node.
 *
 * @param out Packet object to populate with received data.
 * @return True when a packet was dequeued, false otherwise.
 */
[[nodiscard]] bool recv(Packet &out);

} // namespace net

#pragma once
/**
 * @file net_driver.hpp
 * @brief Minimal loopback network driver interface for Lattice IPC.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace net {

/** Identifier for a network node. */
using node_t = int;

/**
 * @brief In‐memory representation of a network packet.
 */
struct Packet {
    node_t                  src_node;  ///< Originating node ID
    std::vector<std::byte>  payload;   ///< Packet payload bytes
};

/**
 * @brief Obtain the identifier for the local node.
 *
 * @return Node ID of this host (stub always returns 0).
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send raw bytes to a remote node.
 *
 * Queues the data into an internal per‐node buffer for loopback testing.
 *
 * @param node Destination node ID.
 * @param data Span of bytes to transmit.
 */
void send(node_t node, std::span<const std::byte> data);

/**
 * @brief Retrieve the next pending packet for the local node.
 *
 * @param out Packet object to populate with received data.
 * @return `true` if a packet was dequeued, `false` if none available.
 */
[[nodiscard]] bool recv(Packet &out);

/**
 * @brief Clear all queued packets across every node.
 */
void reset() noexcept;

} // namespace net

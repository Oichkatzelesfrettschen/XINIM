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
 * @brief Retrieve the next packet queued for @p node.
 *
 * @return Packet data or an empty vector when no packet is available.
 */
[[nodiscard]] std::vector<std::byte> receive(int node);

/** @brief Clear all queued packets across all nodes. */
void reset() noexcept;

} // namespace net

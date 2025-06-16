#pragma once
/**
 * @file net_driver.hpp
 * @brief UDP based network driver interface for Lattice IPC.
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace net {

/** Identifier for a network node. */
using node_t = int;

/**
 * @brief In‐memory representation of a network packet.
 */
struct Packet {
    node_t src_node;                ///< Originating node ID
    std::vector<std::byte> payload; ///< Packet payload bytes
};

/**
 * @brief Network driver configuration for ::init.
 *
 * Setting ``node_id`` to ``0`` instructs the driver to derive a unique
 * identifier by hashing the primary network interface.  A non-zero value
 * overrides the automatic detection and is returned by
 * ::local_node().
 */
struct Config {
    node_t node_id;     ///< Explicit node identifier or ``0`` for auto
    std::uint16_t port; ///< UDP port to bind locally
};

/**
 * Callback invoked whenever a packet is received.
 */
using RecvCallback = std::function<void(const Packet &)>;

/** Initialize the driver with @p cfg opening the UDP socket. */
void init(const Config &cfg);

/** Register a remote @p node reachable at @p host:@p port. */
void add_remote(node_t node, const std::string &host, std::uint16_t port);

/** Install a packet receive callback. */
void set_recv_callback(RecvCallback cb);

/** Stop background networking threads and reset state. */
void shutdown() noexcept;

/**
 * @brief Report the configured node identifier.
 *
 * After calling ::init the driver either returns the user-supplied value
 * or the automatically derived identifier when ``Config::node_id`` was
 * zero.  ``0`` is returned if initialization has not yet occurred.
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send raw bytes to a remote node using UDP.
 *
 * The payload is prefixed with the local node identifier before being
 * transmitted to the remote host registered through ::add_remote. If the
 * destination is unknown the function silently drops the data.
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

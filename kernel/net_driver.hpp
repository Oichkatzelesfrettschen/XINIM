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

/** Configuration options for ::init. */
struct Config {
    node_t node_id;     ///< Local node identifier
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
 * @brief Retrieve the configured or detected node identifier.
 *
 * The driver prefers the identifier specified via ::Config::node_id.
 * When that value is zero, the bound UDP socket is queried through
 * <tt>getsockname</tt> to obtain the IPv4 address of the socket. The
 * address is converted to host byte order and returned. A value of
 * ``0`` denotes that no identifier was configured or detected.
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Transmit raw bytes to a remote node over UDP.
 *
 * The payload is prefixed with the local identifier then sent via
 * <tt>sendto</tt> directly to the host registered through ::add_remote.
 * Unknown destinations result in silent packet drops. No queuing is
 * performed.
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

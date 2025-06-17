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
 * @brief Policy for handling packets when the receive queue is full.
 */
enum class OverflowPolicy {
    Drop,      ///< Discard the newest packet when the queue is full
    Overwrite, ///< Replace the oldest packet when the queue is full
};

/** Configuration options for ::init. */
struct Config {
    node_t node_id;               ///< Local node identifier
    std::uint16_t port;           ///< UDP port to bind locally
    std::size_t max_queue_length; ///< Maximum number of queued packets (0 = unlimited)
    OverflowPolicy overflow;      ///< Overflow behaviour when queue is full

    /// Construct a Config with sensible defaults.
    explicit constexpr Config(node_t node_id_ = 0, std::uint16_t port_ = 0,
                              std::size_t max_queue_length_ = 0,
                              OverflowPolicy overflow_ = OverflowPolicy::Drop) noexcept
        : node_id{node_id_}, port{port_}, max_queue_length{max_queue_length_}, overflow{overflow_} {
    }
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
 * @brief Obtain the network derived identifier for this host.
 *
 * The driver queries the bound UDP socket to read back the IPv4 address.
 * The address is converted to host byte order and returned as the node
 * identifier. A value of ``0`` indicates the address could not be
 * determined.
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

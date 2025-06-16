#pragma once
/**
 * @file net_driver.hpp
 * @brief UDP/TCP network driver interface for Lattice IPC.
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <netinet/in.h>
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
 * @brief Supported transport protocols.
 */
enum class Protocol { UDP, TCP };

/**
 * @brief Description of a remote node.
 */
struct Remote {
    sockaddr_in addr;   ///< Destination address
    Protocol proto;     ///< Transport protocol
    int tcp_socket{-1}; ///< Connected TCP socket when @c proto is TCP
};

/**
 * Callback invoked whenever a packet is received.
 */
using RecvCallback = std::function<void(const Packet &)>;

/** Initialize the driver with @p cfg opening the UDP socket. */
void init(const Config &cfg);

/**
 * @brief Register a remote node.
 *
 * @param node Numeric identifier for the peer.
 * @param host IPv4 address or hostname of the peer.
 * @param port UDP/TCP port number.
 * @param proto Transport protocol used to reach the peer.
 */
void add_remote(node_t node, const std::string &host, std::uint16_t port,
                Protocol proto = Protocol::UDP);

/** Install a packet receive callback. */
void set_recv_callback(RecvCallback cb);

/** Stop background networking threads and reset state. */
void shutdown() noexcept;

/**
 * @brief Obtain the network derived identifier for this host.
 *
 * The driver calls @c gethostname and hashes the resulting string to
 * generate a stable node identifier. A value of ``0`` indicates the
 * hostname could not be retrieved.
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send raw bytes to a remote node.
 *
 * The payload is prefixed with the local node identifier before being
 * transmitted either via UDP datagrams or an established TCP connection
 * depending on the configuration of the remote node. Unknown destinations
 * are ignored.
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

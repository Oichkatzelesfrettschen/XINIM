#pragma once
/**
 * @file net_driver.hpp
 * @brief UDP/TCP network driver interface for Lattice IPC.
 *
 * This driver binds a local UDP port (and TCP listen socket, if enabled)
 * and provides asynchronous send/receive of framed, prefixed packets.
 * Each packet is prefixed with the sender's node ID and delivered over
 * UDP or a transient TCP connection based on peer configuration.
 *
 * Usage:
 *   1. net::init({ node_id, udp_port });
 *   2. net::add_remote(peer_id, "host", port, Protocol::UDP|TCP);
 *   3. net::set_recv_callback(callback)   // optional
 *   4. net::send(peer_id, data);
 *   5. while (net::recv(pkt)) { ... }
 *   6. net::shutdown();
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
    node_t                 src_node;  ///< Originating node ID
    std::vector<std::byte> payload;   ///< Raw payload bytes (post‐prefix)
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
 * @brief Supported transport protocols for remote peers.
 */
enum class Protocol { UDP, TCP };

/**
 * @brief Description of a remote peer.
 *
 * When proto==TCP, send() will establish a transient TCP connection.
 * When proto==UDP, send() uses sendto() on the bound UDP socket.
 */
struct Remote {
    sockaddr_in addr;     ///< IPv4 socket address of the peer
    Protocol     proto;   ///< Protocol to use (UDP or TCP)
    int          tcp_fd{-1}; ///< TCP socket FD (if persistent; optional)
};

/** Callback type invoked on packet arrival. */
using RecvCallback = std::function<void(const Packet&)>;

/**
 * @brief Initialize the network driver.
 *
 * - Binds a UDP socket to cfg.port (INADDR_ANY).
 * - Sets up a TCP listening socket on cfg.port.
 * - Starts background threads for UDP recv and TCP accept.
 *
 * @param cfg Local node configuration.
 * @throws std::system_error on socket/bind errors.
 */
void init(const Config& cfg);

/**
 * @brief Register a remote peer for send().
 *
 * @param node  Numeric identifier of the peer.
 * @param host  IPv4 dotted‐decimal string or hostname.
 * @param port  UDP/TCP port on which peer listens.
 * @param proto Transport protocol to use.
 */
void add_remote(node_t node,
                const std::string& host,
                uint16_t port,
                Protocol proto = Protocol::UDP);

/**
 * @brief Install a receive callback.
 *
 * The callback is invoked from the background threads whenever
 * a packet is enqueued. It is safe to call recv() instead.
 *
 * @param cb Function to call on packet arrival.
 */
void set_recv_callback(RecvCallback cb);

/**
 * @brief Shutdown the network driver and stop all threads.
 *
 * Closes sockets, joins threads, clears queues and peer list.
 */
void shutdown() noexcept;

/**
 * @brief Retrieve the local node identifier.
 *
 * Returns cfg.node_id if nonzero; otherwise calls getsockname()
 * on the bound UDP socket, converting the IPv4 address to host‐order.
 * Returns 0 if detection fails.
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send raw bytes to a registered peer.
 *
 * The payload is prefixed with the local node identifier before being
 * transmitted to the remote host registered through ::add_remote. If the
 * destination is unknown the function returns ``false``.
 * Frames the packet as [ local_node() | payload... ] and transmits
 * via UDP or TCP based on the peer's Protocol.
 * Unknown node IDs are silently ignored.
 *
 * @param node Destination node ID.
 * @param data Span of bytes to transmit.
 * @return ``true`` on success, ``false`` on failure or unknown destination.
 */
[[nodiscard]] bool send(node_t node, std::span<const std::byte> data);

/**
 * @brief Dequeue the next received packet, if any.
 *
 * @param out Packet object to populate.
 * @return true if a packet was dequeued into out, false otherwise.
 */
[[nodiscard]] bool recv(Packet& out);

/**
 * @brief Clear all pending packets across every node.
 *
 * Empties the internal receive queue.
 */
void reset() noexcept;

} // namespace net

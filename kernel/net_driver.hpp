#pragma once
/**
 * @file net_driver.hpp
 * @brief UDP + TCP/IP + IPv4 + SNMP network driver interface built on
 *        Berkeley sockets for Lattice IPC.
 *
 * This driver binds a local UDP port (with an optional TCP listening
 * socket) and provides asynchronous send/receive of framed packets.
 * Each frame is prefixed with the sender's node identifier and is
 * transmitted over UDP or a transient TCP connection using standard
 * Berkeley socket calls.
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
    node_t src_node;                ///< Originating node ID
    std::vector<std::byte> payload; ///< Raw payload bytes (post‐prefix)
};

/**
 * @brief Policy for handling packets when the receive queue is full.
 */
enum class OverflowPolicy {
    DropNewest, ///< Discard the newly received packet when the queue is full
    DropOldest  ///< Replace the oldest queued packet when full
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
    node_t node_id;               ///< Local node identifier
    std::uint16_t port;           ///< UDP port to bind locally
    std::size_t max_queue_length; ///< Maximum number of queued packets (0 = unlimited)
    OverflowPolicy overflow;      ///< Overflow behaviour when queue is full

    /// Construct a Config with sensible defaults.
    explicit constexpr Config(node_t node_id_ = 0, std::uint16_t port_ = 0,
                              std::size_t max_queue_length_ = 0,
                              OverflowPolicy overflow_ = OverflowPolicy::DropNewest) noexcept
        : node_id{node_id_}, port{port_}, max_queue_length{max_queue_length_}, overflow{overflow_} {
    }
};

/**
 * @brief Supported transport protocols for remote peers.
 */
enum class Protocol { UDP, TCP };

/** Callback type invoked on packet arrival. */
using RecvCallback = std::function<void(const Packet &)>;

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
void init(const Config &cfg);

/**
 * @brief Register a remote peer for send().
 *
 * @param node  Numeric identifier of the peer.
 * @param host  IPv4 dotted‐decimal string or hostname.
 * @param port  UDP/TCP port on which peer listens.
 * @param proto Transport protocol to use.
 */
void add_remote(node_t node, const std::string &host, uint16_t port,
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
 * @brief Report the configured node identifier.
 *
 * After calling ::init the driver either returns the user-supplied value
 * or an automatically derived identifier when ``Config::node_id`` equals
 * zero. ``0`` is returned if initialization has not yet occurred.
 *
 * When no identifier is configured the driver enumerates active network
 * interfaces via ``getifaddrs(3)`` and hashes the first non-loopback MAC or
 * IPv4 address found. If detection fails the hostname is hashed as a
 * deterministic fallback.

 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send raw bytes to a registered peer.
 *
 * The payload is prefixed with the local node identifier and transmitted to
 * the host registered with ::add_remote. Unknown destinations are rejected.
 * For ``Protocol::TCP`` the driver establishes a transient connection when no
 * persistent socket exists. Transmission loops until all bytes are sent.
 * Datagram mode uses ``sendto()`` on the UDP socket. No additional queuing
 * occurs in either case.
 *
 * @param node Destination node ID.
 * @param data Span of bytes to transmit.
 * @return ``true`` on success; ``false`` if the peer is unknown or a socket
 *         error occurs.
 */
[[nodiscard]] bool send(node_t node, std::span<const std::byte> data);

/**
 * @brief Dequeue the next received packet, if any.
 *
 * @param out Packet object to populate.
 * @return true if a packet was dequeued into out, false otherwise.
 */
[[nodiscard]] bool recv(Packet &out);

/**
 * @brief Clear all pending packets across every node.
 *
 * Empties the internal receive queue.
 */
void reset() noexcept;

} // namespace net

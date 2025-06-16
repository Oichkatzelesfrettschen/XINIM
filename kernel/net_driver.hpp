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
    node_t                 src_node;  ///< Originating node ID
    std::vector<std::byte> payload;   ///< Packet payload bytes
};

/** Configuration for ::init. */
struct Config {
    node_t      node_id;  ///< Local node identifier
    uint16_t    port;     ///< UDP port to bind locally
};

/** Callback invoked whenever a packet is received. */
using RecvCallback = std::function<void(const Packet&)>;

/**
 * @brief Initialize the network driver.
 *
 * Binds a UDP socket to cfg.port and starts background receiver threads.
 */
void init(const Config& cfg);

/**
 * @brief Register a remote peer.
 *
 * @param node Remote node identifier.
 * @param host IPv4 address or hostname of the peer.
 * @param port UDP/TCP port number.
 * @param tcp  When true, send() will use a transient TCP connection; otherwise UDP.
 */
void add_remote(node_t node,
                const std::string& host,
                uint16_t port,
                bool tcp = false);

/** Install a packet receive callback. */
void set_recv_callback(RecvCallback cb);

/** Shutdown the network driver and stop all background threads. */
void shutdown() noexcept;

/**
 * @brief Derive a stable node ID from the local hostname.
 *
 * Hashes gethostname() into a node_t. Returns 0 on failure.
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send raw bytes to a remote node.
 *
 * Prefixes the payload with the local node ID and transmits via UDP or TCP
 * depending on how the peer was registered. Unknown destinations are ignored.
 */
void send(node_t node, std::span<const std::byte> data);

/**
 * @brief Retrieve the next pending packet for this node.
 *
 * @return true if a packet was dequeued into out, false if none available.
 */
[[nodiscard]] bool recv(Packet& out);

/** Clear all queued packets across every node. */
void reset() noexcept;

} // namespace net

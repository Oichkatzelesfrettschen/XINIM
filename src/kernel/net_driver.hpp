#pragma once
/**
 * @file net_driver.hpp
 * @brief UDP/TCP network driver interface for Lattice IPC (POSIX sockets, C++23).
 *
 * This interface provides asynchronous, multi-threaded UDP/TCP message transport
 * between nodes in a Lattice IPC system. Messages are framed with the sender's
 * node ID and routed using a mutex-protected peer registry.
 *
 * Usage:
 *   net::init({0, 12000});                               // autodetect node_id
 *   net::add_remote(2, "192.168.1.4", 12000, Protocol::TCP);
 *   net::send(2, payload);                               // sends [local_node|payload]
 *   while (net::recv(pkt)) { // process pkt }
 *   net::shutdown();
 *
 * Thread Safety: All APIs are thread-safe and may be called from multiple threads.
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <netinet/in.h>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace net {

/** Integer identifier representing a logical network node. */
using node_t = int;

/**
 * @brief In-memory representation of a framed message.
 *
 * Messages sent via send() are prepended with the sender node ID. On receipt
 * they appear as a Packet with src_node and payload.
 */
struct Packet {
    node_t src_node;                ///< Originating node ID
    std::vector<std::byte> payload; ///< Message payload
};

/**
 * @brief Policy for handling packets when the receive queue is full.
 */
enum class OverflowPolicy { DropNewest, DropOldest };

/**
 * @brief Transport protocol used for a remote peer.
 */
enum class Protocol { UDP, TCP };

/**
 * @brief Network driver configuration structure.
 */
struct Config {
    node_t node_id;               ///< Preferred node identifier (0 = auto-detect)
    std::uint16_t port;           ///< Local UDP/TCP port to bind
    std::size_t max_queue_length; ///< Maximum packets in the receive queue
    OverflowPolicy overflow;      ///< Policy when the receive queue overflows

    constexpr Config(node_t node_id_ = 0, std::uint16_t port_ = 0, std::size_t max_len = 0,
                     OverflowPolicy policy = OverflowPolicy::DropNewest) noexcept
        : node_id(node_id_), port(port_), max_queue_length(max_len), overflow(policy) {}
};

/** Callback type invoked on packet arrival. */
using RecvCallback = std::function<void(const Packet &)>;

/**
 * @brief Initialise networking with the supplied configuration.
 */
void init(const Config &cfg);

/**
 * @brief Register a remote peer for subsequent communication.
 *
 * @param node Logical node identifier.
 * @param host IPv4 address or hostname of the peer.
 * @param port UDP/TCP port number.
 * @param proto Transport protocol to use.
 */
void add_remote(node_t node, const std::string &host, uint16_t port,
                Protocol proto = Protocol::UDP);

/**
 * @brief Install a callback invoked whenever a packet arrives.
 */
void set_recv_callback(RecvCallback cb);

/**
 * @brief Stop all network threads and release socket resources.
 */
void shutdown() noexcept;

/**
 * @brief Return the detected identifier for the local node.
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Transmit a payload to a registered peer.
 *
 * @param node Destination node identifier.
 * @param data Payload bytes to send.
 * @return std::errc{} on success or an error code on failure.
 */
[[nodiscard]] std::errc send(node_t node, std::span<const std::byte> data);

/**
 * @brief Retrieve the next packet from the receive queue.
 *
 * @param out Packet structure receiving the message.
 * @return true if a packet was available.
 */
[[nodiscard]] bool recv(Packet &out);

} // namespace net

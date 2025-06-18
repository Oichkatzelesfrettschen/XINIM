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
 *   while (net::recv(pkt)) { /* process pkt */ }
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
#include <system_error> // for std::errc

namespace net {

/** Integer identifier representing a logical network node. */
using node_t = int;

/**
 * @brief In-memory representation of a framed message.
 *
 * Messages sent via send() are prepended with the sender node ID.  On receipt
 * they appear as a Packet with src_node and payload.
 */
struct Packet {
    node_t                   src_node;  ///< Originating node ID
    std::vector<std::byte>   payload;   ///< Message payload (excluding prefix)
};

/**
 * @brief Policy for handling packets when the receive queue is full.
 */
enum class OverflowPolicy {
    DropNewest, ///< Discard the newly-received packet
    DropOldest  ///< Remove the oldest queued packet to make room
};

/**
 * @brief Transport protocol used for a remote peer.
 */
enum class Protocol {
    UDP, ///< Datagram transport
    TCP  ///< Stream transport (persistent or transient)
};

/**
 * @brief Network driver configuration.
 *
 * @param node_id_        Preferred node ID (0 = auto-detect & persist)
 * @param port_           Local UDP/TCP port to bind
 * @param max_len         Max packets in the receive queue (0 = unlimited)
 * @param policy          Overflow policy
 * @param node_id_dir_    Directory for persisting auto-detected node ID
 */
struct Config {
    node_t                  node_id;
    std::uint16_t           port;
    std::size_t             max_queue_length;
    OverflowPolicy          overflow;
    std::filesystem::path   node_id_dir;

    constexpr Config(node_t                  node_id_        = 0,
                     std::uint16_t           port_           = 0,
                     std::size_t             max_len         = 0,
                     OverflowPolicy          policy          = OverflowPolicy::DropNewest,
                     std::filesystem::path   node_id_dir_    = {}) noexcept
      : node_id(node_id_),
        port(port_),
        max_queue_length(max_len),
        overflow(policy),
        node_id_dir(std::move(node_id_dir_))
    {}
};

/** Callback type invoked on packet arrival (from background thread). */
using RecvCallback = std::function<void(const Packet&)>;

/**
 * @brief Initialize the network driver.
 *
 * Binds dual-stack UDP and TCP sockets on cfg.port and launches background
 * receiver threads.
 *
 * @param cfg Driver configuration
 * @throws std::system_error on socket/bind/thread failures
 */
void init(const Config& cfg);

/**
 * @brief Register a remote peer for sending.
 *
 * Resolves host (IPv4, IPv6, or DNS) and registers the peer.  For TCP, a
 * persistent connection is established immediately.
 *
 * @param node  Logical peer ID
 * @param host  Hostname or IP literal
 * @param port  Port number
 * @param proto Transport protocol
 * @throws std::invalid_argument on name resolution failure
 * @throws std::system_error on TCP socket/connect failure
 */
void add_remote(node_t node,
                const std::string& host,
                uint16_t port,
                Protocol proto = Protocol::UDP);

/**
 * @brief Install a receive callback.
 *
 * Callback will be invoked from the background thread whenever a packet
 * arrives.  Alternatively, applications may poll with recv().
 *
 * @param cb Function to call on packet arrival
 */
void set_recv_callback(RecvCallback cb);

/**
 * @brief Shutdown the network driver.
 *
 * Stops background threads, closes sockets, and clears internal state.
 */
void shutdown() noexcept;

/**
 * @brief Retrieve the stable local node identifier.
 *
 * If cfg.node_id was non-zero, returns that.  Otherwise:
 *   1. Reads node_id_dir/node_id if present
 *   2. Derives from first active non-loopback interface (MAC or IP)
 *   3. Falls back to a hash of hostname
 *
 * If auto-detected, the ID is written to node_id_dir/node_id.
 *
 * @return Stable non-zero node ID (0 only if uninitialized)
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send a framed message to a peer.
 *
 * Frames data as [local_node|payload] and transmits via UDP or TCP.
 *
 * @param node Destination node ID
 * @param data Payload bytes
 * @return std::errc::success on success, or descriptive error code
 * @throws std::system_error on underlying socket failures
 */
[[nodiscard]] std::errc send(node_t node,
                             std::span<const std::byte> data);

/**
 * @brief Dequeue the next received packet, if any.
 *
 * @param out Packet to populate
 * @return true if a packet was dequeued, false if queue was empty
 */
[[nodiscard]] bool recv(Packet& out);

/**
 * @brief Clear all pending packets from the receive queue.
 */
void reset() noexcept;

/**
 * @brief Close sockets to simulate failure (for unit tests).
 */
void simulate_socket_failure() noexcept;

} // namespace net

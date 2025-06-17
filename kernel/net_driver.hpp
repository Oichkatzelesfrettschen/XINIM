#pragma once
/**
 * @file net_driver.hpp
 * @brief UDP/TCP network driver interface for Lattice IPC (POSIX sockets).
 *
 * This interface provides asynchronous and multi-threaded UDP/TCP message transport
 * between nodes in a Lattice IPC system. Messages are framed with the sender's
 * node ID and routed using a mutex-protected peer registry.
 *
 * Usage:
 *   1. net::init({0, 12000}); // autodetect node_id
 *   2. net::add_remote(2, "192.168.1.4", 12000, Protocol::TCP);
 *   3. net::send(2, payload); // sends [local_node|payload]
 *   4. while (net::recv(pkt)) { process(pkt); }
 *   5. net::shutdown();
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
 * Messages sent using ::send() are prepended with the sender node ID and
 * received as a Packet with the `src_node` field and a payload vector.
 */
struct Packet {
    node_t src_node;                ///< Originating node ID
    std::vector<std::byte> payload; ///< Message payload (excluding prefix)
};

/**
 * @brief Policy for handling packets when the receive queue is full.
 */
enum class OverflowPolicy {
    DropNewest, ///< Drop newest received packet when queue is full
    DropOldest  ///< Drop oldest queued packet to make room
};

/**
 * @brief Transport protocol used for a peer connection.
 */
enum class Protocol {
    UDP, ///< Stateless datagram transport
    TCP  ///< Stream transport (persistent or transient connection)
};

/**
 * @brief Network driver configuration structure.
 *
 * This struct defines the initialization parameters for the network stack.
 * Use `node_id = 0` to auto-detect the ID and persist it to `node_id_dir`.
 */
struct Config {
    node_t node_id;               ///< Preferred node identifier (0 = auto-detect)
    std::uint16_t port;           ///< Local port to bind UDP/TCP sockets
    std::size_t max_queue_length; ///< Maximum packets in the receive queue
    OverflowPolicy overflow;      ///< Policy when the receive queue overflows
    /** Directory where the auto-detected node ID is stored. */
    std::filesystem::path node_id_dir;

    constexpr Config(node_t node_id_ = 0, std::uint16_t port_ = 0, std::size_t max_len = 0,
                     OverflowPolicy policy = OverflowPolicy::DropNewest,
                     std::filesystem::path dir_ = {}) noexcept
        : node_id(node_id_), port(port_), max_queue_length(max_len), overflow(policy),
          node_id_dir(std::move(dir_)) {}
};

/**
 * @brief Callback type invoked on packet arrival.
 */
using RecvCallback = std::function<void(const Packet &)>;

/**
 * @brief Initialize the network driver.
 *
 * Initializes dual-stack (IPv4/IPv6) UDP and TCP sockets and launches
 * background receiver threads. The function binds to `cfg.port` for both protocols.
 *
 * @param cfg Driver configuration including port, node ID, overflow policy and
 *            the directory for persisting the identifier.
 * @throws std::system_error on socket failure, bind failure, or thread launch failure.
 */
void init(const Config &cfg);

/**
 * @brief Add a remote peer for subsequent ::send calls.
 *
 * Resolves a host string (IPv4, IPv6, or hostname) and registers the remote node
 * for sending framed messages. TCP peers can reuse persistent sockets.
 *
 * @param node Logical identifier of the remote peer.
 * @param host Hostname or IP address to connect to.
 * @param port Port to use when sending.
 * @param proto Transport protocol (UDP or TCP).
 * @throws std::invalid_argument if the address is invalid.
 * @throws std::system_error if TCP socket or connect fails.
 */
void add_remote(node_t node, const std::string &host, uint16_t port,
                Protocol proto = Protocol::UDP);

/**
 * @brief Register a callback to be invoked on packet arrival.
 *
 * If set, the callback runs in the context of the receiver thread.
 * It is safe to omit this and instead poll with ::recv().
 *
 * @param cb Callback function taking a const Packet&
 */
void set_recv_callback(RecvCallback cb);

/**
 * @brief Shutdown the network driver and clean up all resources.
 *
 * Closes all sockets, terminates background threads, clears peer state and queue.
 */
void shutdown() noexcept;

/**
 * @brief Returns the local node identifier.
 *
 * If a node ID was provided in Config, that value is returned.
 * Otherwise, the following procedure is used:
 *   1. Try reading `node_id_dir/node_id`
 *   2. Derive from first active non-loopback interface (MAC or IP)
 *   3. Fallback to hash of hostname
 *
 * When executed without root privileges and `node_id_dir` is empty, the
 * directory defaults to `$XDG_STATE_HOME/xinim` or `$HOME/.xinim`.
 *
 * @return Stable integer node identifier (never 0 unless initialization failed).
 */
[[nodiscard]] node_t local_node() noexcept;

/**
 * @brief Send a framed message to the specified peer.
 *
 * Frames the payload as `[local_node | data...]` and transmits over UDP
 * or TCP. For TCP, the function will establish a new socket if needed.
 *
 * @param node Destination node ID
 * @param data Payload to send (without prefix)
 * @return std::errc::success on success or a descriptive error code
 * @throws std::system_error for POSIX socket failures (send, connect, etc)
 */
[[nodiscard]] std::errc send(node_t node, std::span<const std::byte> data);

/**
 * @brief Retrieve the next available incoming packet.
 *
 * @param out Packet structure to fill in.
 * @return true if a packet was retrieved, false if queue is empty.
 */
[[nodiscard]] bool recv(Packet &out);

/**
 * @brief Clear all pending packets in the receive queue.
 */
void reset() noexcept;

} // namespace net

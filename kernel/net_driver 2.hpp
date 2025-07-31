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
#include <system_error> // for std::errc
#include <vector>

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
    node_t src_node;                ///< Originating node ID
    std::vector<std::byte> payload; ///< Message payload (excluding prefix)
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
    node_t node_id;
    std::uint16_t port;
    std::size_t max_queue_length;
    OverflowPolicy overflow;
    std::filesystem::path node_id_dir;

    constexpr Config(node_t node_id_ = 0, std::uint16_t port_ = 0, std::size_t max_len = 0,
                     OverflowPolicy policy = OverflowPolicy::DropNewest,
                     std::filesystem::path node_id_dir_ = {}) noexcept
        : node_id(node_id_), port(port_), max_queue_length(max_len), overflow(policy),
          node_id_dir(std::move(node_id_dir_)) {}
};

/** Callback type invoked on packet arrival (from background thread). */
using RecvCallback = std::function<void(const Packet &)>;

/**
 * @brief Initialize the network driver.
 *
 * Binds dual-stack UDP and TCP sockets on cfg.port and launches background
 * receiver threads.
 *
 * @param cfg Driver configuration
 * @throws std::system_error on socket/bind/thread failures
 */
void init(const Config &cfg);

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
void add_remote(node_t node, const std::string &host, uint16_t port,
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
[[nodiscard]] std::errc send(node_t node, std::span<const std::byte> data);

/**
 * @brief Dequeue the next received packet, if any.
 *
 * @param out Packet to populate
 * @return true if a packet was dequeued, false if queue was empty
 */
[[nodiscard]] bool recv(Packet &out);

/**
 * @brief Clear all pending packets from the receive queue.
 */
void reset() noexcept;

/**
 * @brief Close sockets to simulate failure (for unit tests).
 */
void simulate_socket_failure() noexcept;

} // namespace net
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
 *   1. net::driver.init({0, 12000});
 *   2. net::driver.add_remote(2, "192.168.1.4", 12000, Protocol::TCP);
 *   3. net::driver.send(2, payload); // sends [local_node|payload]
 *   4. while (net::driver.recv(pkt)) { process(pkt); }
 *   5. net::driver.shutdown();
 *
 * Thread Safety: All APIs are thread-safe and may be called from multiple threads.
 */

#include <cstddef>
#include <cstdint>
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
 * Use `node_id = 0` to auto-detect the ID and persist it to `/etc/xinim/node_id`.
 */
struct Config {
    node_t node_id;               ///< Preferred node identifier (0 = auto-detect)
    std::uint16_t port;           ///< Local port to bind UDP/TCP sockets
    std::size_t max_queue_length; ///< Maximum packets in the receive queue
    OverflowPolicy overflow;      ///< Policy when the receive queue overflows

    constexpr Config(node_t node_id_ = 0, std::uint16_t port_ = 0, std::size_t max_len = 0,
                     OverflowPolicy policy = OverflowPolicy::DropNewest) noexcept
        : node_id(node_id_), port(port_), max_queue_length(max_len), overflow(policy) {}
};

/**
 * @brief Callback type invoked on packet arrival.
 */
using RecvCallback = std::function<void(const Packet &)>;

/**
 * @brief Networking driver managing sockets, queues, and threads.
 */
class Driver {
  public:
    Driver() = default;
    ~Driver();

    /**
     * @brief Initialize the driver and spawn worker threads.
     *
     * @param cfg Driver configuration including port, node ID, and overflow policy.
     * @throws std::system_error on socket or thread failure.
     */
    void init(const Config &cfg);

    /**
     * @brief Register a remote peer for subsequent transmissions.
     *
     * @param node  Logical identifier of the remote peer.
     * @param host  Hostname or IP address to connect to.
     * @param port  Port number for sending.
     * @param proto Transport protocol.
     */
    void add_remote(node_t node, const std::string &host, uint16_t port,
                    Protocol proto = Protocol::UDP);

    /**
     * @brief Register a callback invoked on packet arrival.
     *
     * @param cb Callback function executed by the receive thread.
     */
    void set_recv_callback(RecvCallback cb);

    /**
     * @brief Shut down the driver and release all resources.
     */
    void shutdown() noexcept;

    /**
     * @brief Return the local node identifier.
     */
    [[nodiscard]] node_t local_node() noexcept;

    /**
     * @brief Send a framed message to the specified peer.
     */
    [[nodiscard]] std::errc send(node_t node, std::span<const std::byte> data);

    /**
     * @brief Retrieve the next queued packet.
     */
    [[nodiscard]] bool recv(Packet &out);

    /**
     * @brief Clear all pending packets from the queue.
     */
    void reset() noexcept;

  private:
    Config cfg_{};
    int udp_sock_{-1};
    int tcp_listen_{-1};

    struct Remote {
        sockaddr_storage addr{};
        socklen_t addr_len{};
        Protocol proto{};
        int tcp_fd{-1};
    };

    std::unordered_map<node_t, Remote> remotes_;
    std::mutex remotes_mutex_;

    std::deque<Packet> queue_;
    std::mutex mutex_;
    RecvCallback callback_;
    std::atomic<bool> running_{false};
    std::jthread udp_thread_;
    std::jthread tcp_thread_;

    [[nodiscard]] static bool connection_lost(int err) noexcept;
    void reconnect_tcp(Remote &rem);
    [[nodiscard]] static std::vector<std::byte> frame_payload(std::span<const std::byte> data);
    void enqueue_packet(Packet &&pkt);
    void udp_recv_loop();
    void tcp_accept_loop();
};

/// Global driver instance replacing previous static variables
extern Driver driver;

} // namespace net

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

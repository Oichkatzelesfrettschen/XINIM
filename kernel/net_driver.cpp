/**
 * @file net_driver.cpp
 * @brief Robust UDP/TCP networking backend for Lattice IPC (IPv4/IPv6, C++23).
 * @warning Lacks zero-copy optimizations; consider io_uring for future refactor.
 */

#include "net_driver.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h> // for sockaddr_in
#include <netpacket/packet.h>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net {
namespace {

/** Active configuration. */
static Config g_cfg{};
/** UDP socket descriptor. */
static int g_udp_sock = -1;
/** TCP listen socket descriptor. */
static int g_tcp_listen = -1;
/**
 * Represents a remote peer with UDP or persistent TCP transport.
 */
struct Remote {
    sockaddr_in addr{};
    Protocol proto = Protocol::UDP;
    int tcp_fd = -1; ///< valid if proto==TCP
};
/** Peer registry: node_id → Remote. */
static std::unordered_map<node_t, Remote> g_remotes;
/** Incoming packet queue. */
static std::deque<Packet> g_queue;
/** Protects g_queue. */
static std::mutex g_mutex;
/** Optional receive‐callback. */
static RecvCallback g_callback;
/** Controls I/O threads. */
static std::atomic<bool> g_running{false};
/** Background threads for UDP & TCP. */
static std::jthread g_udp_thread, g_tcp_thread;

/**
 * Frame data as [ local_node | payload... ].
 */
static std::vector<std::byte> frame_payload(std::span<const std::byte> data) {
    node_t nid = local_node();
    std::vector<std::byte> buf(sizeof(nid) + data.size());
    std::memcpy(buf.data(), &nid, sizeof(nid));
    std::memcpy(buf.data() + sizeof(nid), data.data(), data.size());
    return buf;
}

/**
 * Enqueue a packet with overflow policy.
 */
static void enqueue_packet(Packet &&pkt) {
    std::lock_guard lock{g_mutex};
    if (g_cfg.max_queue_length > 0 &&
        g_queue.size() >= static_cast<size_t>(g_cfg.max_queue_length)) {
        if (g_cfg.overflow == OverflowPolicy::DropOldest) {
            g_queue.pop_front();
        } else { // OverwriteOldest
            g_queue.pop_front();
        }
    }
    g_queue.push_back(std::move(pkt));
    if (g_callback) {
        g_callback(g_queue.back());
    }
}

/**
 * UDP receive loop: dequeue from socket → enqueue_packet.
 */
void udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        ssize_t n = ::recvfrom(g_udp_sock, buf.data(), buf.size(), 0,
                               reinterpret_cast<sockaddr *>(&peer), &len);
        if (n <= static_cast<ssize_t>(sizeof(node_t) * 2))
            continue;
        Packet pkt;
        std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
        pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
        enqueue_packet(std::move(pkt));
    }
}

/**
 * TCP accept loop: accept, read full frames, then enqueue_packet.
 */
void tcp_accept_loop() {
    ::listen(g_tcp_listen, 8);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        int client = ::accept(g_tcp_listen, reinterpret_cast<sockaddr *>(&peer), &len);
        if (client < 0)
            continue;
        std::array<std::byte, 2048> buf;
        while (true) {
            ssize_t n = ::recv(client, buf.data(), buf.size(), 0);
            if (n <= static_cast<ssize_t>(sizeof(node_t)))
                break;
            Packet pkt;
            std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
            pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
            enqueue_packet(std::move(pkt));
        }
        ::close(client);
    }
}

} // namespace

/**
 * @brief Initialize sockets and launch networking threads.
 *
 * Creates UDP and TCP sockets, binds them to the configured port and starts
 * background threads for receiving and accepting connections.
 *
 * @param cfg Configuration parameters controlling socket ports and queue
 *            behaviour.
 * @throws std::system_error If socket creation or binding fails.
 * @pre Network interfaces are up and accessible.
 * @post I/O threads are spawned and sockets bound to @p cfg.port.
 * @warning No hardware IRQ handling; relies solely on OS-level buffering.
 */
void init(const Config &cfg) {
    g_cfg = cfg;

    // UDP socket setup
    g_udp_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (g_udp_sock < 0)
        throw std::system_error(errno, std::generic_category(), "UDP socket");
    sockaddr_in addr{.sin_family = AF_INET,
                     .sin_port = htons(cfg.port),
                     .sin_addr = {.s_addr = htonl(INADDR_ANY)}};
    if (::bind(g_udp_sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        throw std::system_error(errno, std::generic_category(), "UDP bind");

    // TCP listen socket
    g_tcp_listen = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_tcp_listen < 0)
        throw std::system_error(errno, std::generic_category(), "TCP socket");
    int opt = 1;
    ::setsockopt(g_tcp_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (::bind(g_tcp_listen, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        throw std::system_error(errno, std::generic_category(), "TCP bind");

    g_running.store(true, std::memory_order_relaxed);
    g_udp_thread = std::jthread{udp_recv_loop};
    g_tcp_thread = std::jthread{tcp_accept_loop};
}

/**
 * @brief Shut down networking and release resources.
 */
void shutdown() noexcept {
    g_running.store(false, std::memory_order_relaxed);
    if (g_udp_sock != -1) {
        ::close(g_udp_sock);
        g_udp_sock = -1;
    }
    if (g_tcp_listen != -1) {
        ::shutdown(g_tcp_listen, SHUT_RDWR);
        ::close(g_tcp_listen);
        g_tcp_listen = -1;
    }
    if (g_udp_thread.joinable())
        g_udp_thread.join();
    if (g_tcp_thread.joinable())
        g_tcp_thread.join();
    {
        std::lock_guard lock{g_mutex};
        g_queue.clear();
    }
    for (auto &[_, r] : g_remotes) {
        if (r.proto == Protocol::TCP && r.tcp_fd >= 0)
            ::close(r.tcp_fd);
    }
    g_remotes.clear();
    g_callback = nullptr;
}

/**
 * @brief Register a remote peer for communication.
 *
 * @param node Logical identifier of the remote peer.
 * @param host Hostname or IPv4 address of the peer.
 * @param port Port number to use for communication.
 * @param proto Transport protocol (UDP or TCP).
 * @throws std::invalid_argument If the host address cannot be parsed.
 * @throws std::system_error On TCP socket or connect failure.
 */
void add_remote(node_t node, const std::string &host, uint16_t port, Protocol proto) {
    Remote rem{};
    rem.proto = proto;
    rem.addr.sin_family = AF_INET;
    rem.addr.sin_port = htons(port);
    if (::inet_aton(host.c_str(), &rem.addr.sin_addr) == 0)
        throw std::invalid_argument("net_driver: bad host address");
    if (proto == Protocol::TCP) {
        rem.tcp_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (rem.tcp_fd < 0 ||
            ::connect(rem.tcp_fd, reinterpret_cast<sockaddr *>(&rem.addr), sizeof(rem.addr)) != 0) {
            if (rem.tcp_fd >= 0)
                ::close(rem.tcp_fd);
            throw std::system_error(errno, std::generic_category(), "TCP connect");
        }
    }
    g_remotes[node] = rem;
}

/**
 * @brief Install a callback invoked when a packet arrives.
 */
void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

/**
 * @brief Return the detected local node identifier.
 */
node_t local_node() noexcept {
    if (g_cfg.node_id != 0)
        return g_cfg.node_id;
    if (g_udp_sock >= 0) {
        sockaddr_in sa{};
        socklen_t len = sizeof(sa);
        if (::getsockname(g_udp_sock, reinterpret_cast<sockaddr *>(&sa), &len) == 0) {
            node_t id = static_cast<node_t>(ntohl(sa.sin_addr.s_addr) & 0x7fffffff);
            return id ? id : 1;
        }
    }
    char host[256]{};
    if (::gethostname(host, sizeof(host)) == 0) {
        auto h = std::hash<std::string_view>{}(host);
        node_t id = static_cast<node_t>(h & 0x7fffffff);
        return id ? id : 1;
    }
    return 1;
}

/**
 * @brief Send a framed message to a registered peer.
 *
 * @param node Destination node identifier.
 * @param data Payload bytes to transmit.
 * @return std::errc{} on success, otherwise an error code.
 */
std::errc send(node_t node, std::span<const std::byte> data) {
    auto it = g_remotes.find(node);
    if (it == g_remotes.end()) {
        return std::errc::host_unreachable;
    }
    auto buf = frame_payload(data);
    auto &rem = it->second;
    ssize_t n = 0;
    if (rem.proto == Protocol::TCP && rem.tcp_fd >= 0) {
        n = ::send(rem.tcp_fd, buf.data(), buf.size(), 0);
    } else {
        n = ::sendto(g_udp_sock, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr *>(&rem.addr),
                     sizeof(rem.addr));
    }
    if (n != static_cast<ssize_t>(buf.size())) {
        return std::errc::io_error;
    }
    return {};
}

/**
 * @brief Retrieve the next packet from the receive queue.
 *
 * @param out Packet object that receives the data.
 * @return true if a packet was available.
 */
bool recv(Packet &out) {
    std::lock_guard lock{g_mutex};
    if (g_queue.empty())
        return false;
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

} // namespace net

/**
 * @file net_driver.cpp
 * @brief Robust UDP/TCP network driver for Lattice IPC.
 *
 * Implements:
 *  - Configurable receive‐queue overflow policy
 *  - Persistent or transient TCP peers
 *  - Framing, background I/O, callbacks, and blocking/non-blocking APIs
 */

#include "net_driver.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <iostream>
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
struct Peer {
    sockaddr_in addr{};
    Protocol proto = Protocol::UDP;
    int tcp_fd = -1; ///< valid if proto==TCP
};
/** Peer registry: node_id → Peer. */
static std::unordered_map<node_t, Peer> g_remotes;
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
 * @brief Enqueue a packet applying the configured overflow policy.
 *
 * When the queue length reaches @ref Config::max_queue_length this
 * function either drops the incoming packet or overwrites the oldest
 * queued packet according to @ref Config::overflow.  A warning is
 * printed to stderr whenever a packet is discarded or replaced.
 *
 * @param pkt Packet to append.
 */
static void enqueue_packet(Packet &&pkt) {
    std::lock_guard lock{g_mutex};
    if (g_cfg.max_queue_length > 0 &&
        g_queue.size() >= static_cast<size_t>(g_cfg.max_queue_length)) {
        if (g_cfg.overflow == OverflowPolicy::Drop) {
            std::cerr << "net_driver: queue full, dropping packet" << std::endl;
            return;
        }
        std::cerr << "net_driver: queue full, overwriting oldest packet" << std::endl;
        g_queue.pop_front();
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

    // Start I/O threads
    g_running.store(true, std::memory_order_relaxed);
    g_udp_thread = std::jthread{udp_recv_loop};
    g_tcp_thread = std::jthread{tcp_accept_loop};
}

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

void add_remote(node_t node, const std::string &host, uint16_t port, Protocol proto) {
    Peer rem{};
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

void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

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

bool send(node_t node, std::span<const std::byte> data) {
    auto it = g_remotes.find(node);
    if (it == g_remotes.end())
        return false;
    auto buf = frame_payload(data);
    auto &rem = it->second;
    if (rem.proto == Protocol::TCP && rem.tcp_fd >= 0) {
        return ::send(rem.tcp_fd, buf.data(), buf.size(), 0) == static_cast<ssize_t>(buf.size());
    }
    return ::sendto(g_udp_sock, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr *>(&rem.addr),
                    sizeof(rem.addr)) == static_cast<ssize_t>(buf.size());
}

bool recv(Packet &out) {
    std::lock_guard lock{g_mutex};
    if (g_queue.empty())
        return false;
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

void reset() noexcept {
    std::lock_guard lock{g_mutex};
    g_queue.clear();
}

} // namespace net

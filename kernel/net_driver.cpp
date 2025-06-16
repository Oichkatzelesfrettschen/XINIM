/**
 * @file net_driver.cpp
 * @brief UDP/TCP network driver for Lattice IPC.
 */

#include "net_driver.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <array>
#include <cstring>
#include <deque>
#include <mutex>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net {
namespace {

/** Active configuration (node_id and port). */
static Config                      g_cfg{};
/** UDP socket for incoming datagrams. */
static int                         g_udp_sock   = -1;
/** TCP listening socket (for incoming TCP-based IPC). */
static int                         g_tcp_listen = -1;

/**
 * Represents a remote peer: address + transport flag.
 */
struct Remote {
    sockaddr_in addr{};
    bool         use_tcp = false;
};

/** Mapping from node ID to remote endpoint info. */
static std::unordered_map<node_t, Remote> g_remotes;
/** Received packets queue. */
static std::deque<Packet>                g_queue;
/** Protects g_queue. */
static std::mutex                         g_mutex;
/** User-provided receive callback. */
static RecvCallback                       g_callback;
/** Controls background receiver threads. */
static std::atomic<bool>                  g_running{false};
/** Background receiver threads. */
static std::jthread                       g_udp_thread, g_tcp_thread;

/**
 * @brief Background loop polling g_udp_sock for datagrams.
 */
void udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        ssize_t  n   = ::recvfrom(g_udp_sock,
                                  buf.data(),
                                  buf.size(),
                                  0,
                                  reinterpret_cast<sockaddr*>(&peer),
                                  &len);
        if (n <= 0) {
            continue;
        }
        Packet pkt;
        std::memcpy(&pkt.src_node, buf.data(), sizeof(node_t));
        pkt.payload.assign(buf.begin() + sizeof(node_t),
                           buf.begin() + n);

        {
            std::lock_guard lock{g_mutex};
            g_queue.push_back(pkt);
        }
        if (g_callback) {
            g_callback(pkt);
        }
    }
}

/**
 * @brief Background loop accepting TCP connections.
 */
void tcp_accept_loop() {
    ::listen(g_tcp_listen, 8);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t   len = sizeof(peer);
        int client = ::accept(g_tcp_listen,
                              reinterpret_cast<sockaddr*>(&peer),
                              &len);
        if (client < 0) {
            continue;
        }
        std::array<std::byte, 2048> buf;
        ssize_t n;
        while ((n = ::recv(client, buf.data(), buf.size(), 0)) > 0) {
            Packet pkt;
            std::memcpy(&pkt.src_node, buf.data(), sizeof(node_t));
            pkt.payload.assign(buf.begin() + sizeof(node_t),
                               buf.begin() + n);
            {
                std::lock_guard lock{g_mutex};
                g_queue.push_back(pkt);
            }
            if (g_callback) {
                g_callback(pkt);
            }
        }
        ::close(client);
    }
}

} // anonymous namespace

void init(const Config &cfg) {
    g_cfg = cfg;

    // UDP socket
    g_udp_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (g_udp_sock < 0) {
        throw std::system_error(errno, std::generic_category(), "UDP socket");
    }
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(cfg.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(g_udp_sock,
               reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "UDP bind");
    }

    // TCP listening socket
    g_tcp_listen = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_tcp_listen < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP socket");
    }
    int opt = 1;
    ::setsockopt(g_tcp_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (::bind(g_tcp_listen,
               reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "TCP bind");
    }

    // Start background threads
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
    if (g_udp_thread.joinable()) g_udp_thread.join();
    if (g_tcp_thread.joinable()) g_tcp_thread.join();
    {
        std::lock_guard lock{g_mutex};
        g_queue.clear();
    }
    g_remotes.clear();
    g_callback = nullptr;
}

void add_remote(node_t node,
                const std::string &host,
                uint16_t port,
                Protocol proto)
{
    Remote rem{};
    rem.use_tcp = (proto == Protocol::TCP);
    rem.addr.sin_family = AF_INET;
    rem.addr.sin_port   = htons(port);
    if (::inet_aton(host.c_str(), &rem.addr.sin_addr) == 0) {
        return;  // invalid address
    }
    g_remotes[node] = rem;
}

void set_recv_callback(RecvCallback cb) {
    g_callback = std::move(cb);
}

node_t local_node() noexcept {
    std::array<char, 256> host{};
    if (::gethostname(host.data(), host.size()) == 0) {
        std::string_view sv{host.data()};
        auto h = std::hash<std::string_view>{}(sv);
        // ensure non-zero
        return static_cast<node_t>((h & 0x7fffffff) ?: 1);
    }
    return 1;
}

void send(node_t node, std::span<const std::byte> data) {
    auto it = g_remotes.find(node);
    if (it == g_remotes.end()) {
        return;  // unknown destination
    }
    // Frame: [ local_node | payload... ]
    std::vector<std::byte> buf(sizeof(node_t) + data.size());
    std::memcpy(buf.data(), &g_cfg.node_id, sizeof(node_t));
    std::memcpy(buf.data() + sizeof(node_t), data.data(), data.size());

    const auto &rem = it->second;
    if (rem.use_tcp) {
        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return;
        if (::connect(sock,
                      reinterpret_cast<const sockaddr*>(&rem.addr),
                      sizeof(rem.addr)) == 0)
        {
            ::send(sock, buf.data(), buf.size(), 0);
        }
        ::close(sock);
    } else {
        ::sendto(g_udp_sock,
                 buf.data(),
                 buf.size(),
                 0,
                 reinterpret_cast<const sockaddr*>(&rem.addr),
                 sizeof(rem.addr));
    }
}

bool recv(Packet &out) {
    std::lock_guard lock{g_mutex};
    if (g_queue.empty()) {
        return false;
    }
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

void reset() noexcept {
    std::lock_guard lock{g_mutex};
    g_queue.clear();
}

} // namespace net

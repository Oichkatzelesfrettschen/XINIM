/**
 * @file net_driver.cpp
 * @brief UDP based network driver for Lattice IPC.
 */

#include "net_driver.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace net {
namespace {
Config g_cfg{};   //!< active configuration
int g_socket{-1}; //!< UDP socket descriptor

struct RemoteEndpoint {
    sockaddr_in addr{}; //!< Target address for the peer
    bool tcp{false};    //!< Use TCP instead of UDP
};

std::unordered_map<node_t, RemoteEndpoint> g_remotes; //!< node mapping table
std::deque<Packet> g_queue;                           //!< received packets
std::mutex g_mutex;                                   //!< protects g_queue
RecvCallback g_callback;                              //!< user callback
std::jthread g_thread;                                //!< background UDP receiver
int g_tcp_listen{-1};                                 //!< TCP listening socket
std::jthread g_tcp_thread;                            //!< background TCP acceptor
std::atomic<bool> g_running{false};

/**
 * @brief Background loop polling @c g_socket for datagrams.
 */
void recv_loop() {
    std::array<std::byte, 2048> buf{};
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const auto n = ::recvfrom(g_socket, buf.data(), buf.size(), 0,
                                  reinterpret_cast<sockaddr *>(&peer), &len);
        if (n <= 0) {
            continue;
        }
        Packet pkt{};
        std::memcpy(&pkt.src_node, buf.data(), sizeof(node_t));
        pkt.payload.assign(buf.begin() + sizeof(node_t), buf.begin() + n);

        {
            std::lock_guard<std::mutex> lock{g_mutex};
            g_queue.push_back(pkt);
        }
        if (g_callback) {
            g_callback(pkt);
        }
    }
}

/**
 * @brief Accepts TCP connections and processes incoming packets.
 */
void tcp_loop() {
    ::listen(g_tcp_listen, 8);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const int client = ::accept(g_tcp_listen, reinterpret_cast<sockaddr *>(&peer), &len);
        if (client < 0) {
            continue;
        }
        std::array<std::byte, 2048> buf{};
        for (;;) {
            const auto n = ::recv(client, buf.data(), buf.size(), 0);
            if (n <= 0) {
                break;
            }
            Packet pkt{};
            std::memcpy(&pkt.src_node, buf.data(), sizeof(node_t));
            pkt.payload.assign(buf.begin() + sizeof(node_t), buf.begin() + n);
            {
                std::lock_guard<std::mutex> lock{g_mutex};
                g_queue.push_back(pkt);
            }
            if (g_callback) {
                g_callback(pkt);
            }
        }
        ::close(client);
    }
}
} // namespace

void init(const Config &cfg) {
    g_cfg = cfg;
    g_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(g_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error{"bind failed"};
    }

    g_tcp_listen = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_tcp_listen < 0) {
        throw std::runtime_error{"tcp socket"};
    }
    if (::bind(g_tcp_listen, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error{"tcp bind failed"};
    }

    g_running.store(true, std::memory_order_relaxed);
    g_thread = std::jthread{recv_loop};
    g_tcp_thread = std::jthread{tcp_loop};
}

void add_remote(node_t node, const std::string &host, std::uint16_t port, bool tcp) {
    RemoteEndpoint ep{};
    ep.addr.sin_family = AF_INET;
    ep.addr.sin_port = htons(port);
    ::inet_aton(host.c_str(), &ep.addr.sin_addr);
    ep.tcp = tcp;
    g_remotes[node] = ep;
}

void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

node_t local_node() noexcept {
    std::array<char, 256> host{};
    if (::gethostname(host.data(), host.size()) == 0) {
        const std::string_view name{host.data()};
        return static_cast<node_t>(std::hash<std::string_view>{}(name) & 0x7fffffff);
    }
    return 0;
}

void send(node_t node, std::span<const std::byte> data) {
    auto it = g_remotes.find(node);
    if (it == g_remotes.end()) {
        return; // unknown destination
    }
    std::vector<std::byte> buf(sizeof(node_t) + data.size());
    std::memcpy(buf.data(), &g_cfg.node_id, sizeof(node_t));
    std::memcpy(buf.data() + sizeof(node_t), data.data(), data.size());
    if (it->second.tcp) {
        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return;
        }
        if (::connect(sock, reinterpret_cast<const sockaddr *>(&it->second.addr),
                      sizeof(sockaddr_in)) < 0) {
            ::close(sock);
            return;
        }
        ::send(sock, buf.data(), buf.size(), 0);
        ::close(sock);
    } else {
        ::sendto(g_socket, buf.data(), buf.size(), 0,
                 reinterpret_cast<const sockaddr *>(&it->second.addr), sizeof(sockaddr_in));
    }
}

bool recv(Packet &out) {
    std::lock_guard<std::mutex> lock{g_mutex};
    if (g_queue.empty()) {
        return false;
    }
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

void reset() noexcept {
    std::lock_guard<std::mutex> lock{g_mutex};
    g_queue.clear();
}

void shutdown() noexcept {
    g_running.store(false, std::memory_order_relaxed);
    if (g_socket != -1) {
        ::close(g_socket);
        g_socket = -1;
    }
    if (g_tcp_listen != -1) {
        ::shutdown(g_tcp_listen, SHUT_RDWR);
        ::close(g_tcp_listen);
        g_tcp_listen = -1;
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
    if (g_tcp_thread.joinable()) {
        g_tcp_thread.join();
    }
    {
        std::lock_guard<std::mutex> lock{g_mutex};
        g_queue.clear();
    }
    g_remotes.clear();
}

} // namespace net

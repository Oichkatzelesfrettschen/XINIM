/**
 * @file net_driver.cpp
 * @brief UDP/TCP network driver for Lattice IPC.
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
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net {
namespace {
Config g_cfg{};                               //!< active configuration
int g_udp_socket{-1};                         //!< UDP socket descriptor
std::unordered_map<node_t, Remote> g_remotes; //!< node mapping table
std::deque<Packet> g_queue;                   //!< received packets
std::mutex g_mutex;                           //!< protects g_queue
RecvCallback g_callback;                      //!< user callback
std::jthread g_thread;                        //!< background receiver
std::atomic<bool> g_running{false};

/**
 * @brief Background loop polling @c g_udp_socket for datagrams.
 */
void recv_loop() {
    std::array<std::byte, 2048> buf{};
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const auto n = ::recvfrom(g_udp_socket, buf.data(), buf.size(), 0,
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
} // namespace

void init(const Config &cfg) {
    g_cfg = cfg;
    g_udp_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(g_udp_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error{"bind failed"};
    }
    g_running.store(true, std::memory_order_relaxed);
    g_thread = std::jthread{recv_loop};
}

void add_remote(node_t node, const std::string &host, std::uint16_t port, Protocol proto) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_aton(host.c_str(), &addr.sin_addr);

    Remote rem{addr, proto};
    if (proto == Protocol::TCP) {
        rem.tcp_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (::connect(rem.tcp_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            ::close(rem.tcp_socket);
            rem.tcp_socket = -1;
        }
    }
    g_remotes[node] = rem;
}

void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

node_t local_node() noexcept {
    char host[256]{};
    if (::gethostname(host, sizeof(host)) == 0) {
        std::uint32_t hash = 2166136261u;
        for (char c : std::string_view{host}) {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 16777619u;
        }
        return static_cast<node_t>(hash);
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

    if (it->second.proto == Protocol::UDP) {
        ::sendto(g_udp_socket, buf.data(), buf.size(), 0,
                 reinterpret_cast<const sockaddr *>(&it->second.addr), sizeof(sockaddr_in));
    } else if (it->second.tcp_socket != -1) {
        ::send(it->second.tcp_socket, buf.data(), buf.size(), 0);
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
    if (g_udp_socket != -1) {
        ::close(g_udp_socket);
        g_udp_socket = -1;
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
    {
        std::lock_guard<std::mutex> lock{g_mutex};
        g_queue.clear();
    }
    for (auto &[id, rem] : g_remotes) {
        if (rem.tcp_socket != -1) {
            ::close(rem.tcp_socket);
        }
    }
    g_remotes.clear();
}

} // namespace net

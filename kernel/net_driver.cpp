/**
 * @file net_driver.cpp
 * @brief Robust UDP/TCP networking backend for Lattice IPC (IPv4/IPv6, C++23).
 */

#include "net_driver.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
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

constexpr char DEFAULT_NODE_ID_FILE[] = "/etc/xinim/node_id";

static Config g_cfg{};
static int g_udp_sock = -1;
static int g_tcp_listen = -1;

struct Remote {
    sockaddr_storage addr{};
    socklen_t addr_len{};
    Protocol proto{};
    int tcp_fd = -1;
};

static std::unordered_map<node_t, Remote> g_remotes;
static std::mutex g_remotes_mutex;

static std::deque<Packet> g_queue;
static std::mutex g_mutex;
static RecvCallback g_callback;
static std::atomic<bool> g_running{false};
static std::jthread g_udp_thread, g_tcp_thread;

[[nodiscard]] static bool connection_lost(int err) noexcept {
    return err == EPIPE || err == ECONNRESET || err == ENOTCONN || err == ECONNABORTED;
}

static void reconnect_tcp(Remote &rem) {
    if (rem.tcp_fd >= 0)
        ::close(rem.tcp_fd);
    rem.tcp_fd = ::socket(rem.addr.ss_family, SOCK_STREAM, 0);
    if (rem.tcp_fd < 0)
        throw std::system_error(errno, std::generic_category(), "net_driver: socket");
    if (::connect(rem.tcp_fd, reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len) != 0) {
        int err = errno;
        ::close(rem.tcp_fd);
        rem.tcp_fd = -1;
        throw std::system_error(err, std::generic_category(), "net_driver: connect");
    }
}

[[nodiscard]] static std::vector<std::byte> frame_payload(std::span<const std::byte> data) {
    node_t nid = local_node();
    std::vector<std::byte> buf(sizeof(nid) + data.size());
    std::memcpy(buf.data(), &nid, sizeof(nid));
    std::memcpy(buf.data() + sizeof(nid), data.data(), data.size());
    return buf;
}

static void enqueue_packet(Packet &&pkt) {
    std::lock_guard lock{g_mutex};
    if (g_cfg.max_queue_length > 0 && g_queue.size() >= g_cfg.max_queue_length) {
        if (g_cfg.overflow == OverflowPolicy::DropNewest)
            return;
        g_queue.pop_front(); // DropOldest
    }
    g_queue.push_back(std::move(pkt));
    if (g_callback)
        g_callback(g_queue.back());
}

static void udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_storage peer{};
        socklen_t len = sizeof(peer);
        ssize_t n = ::recvfrom(g_udp_sock, buf.data(), buf.size(), 0,
                               reinterpret_cast<sockaddr *>(&peer), &len);
        if (n <= static_cast<ssize_t>(sizeof(node_t)))
            continue;

        Packet pkt;
        std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
        pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
        enqueue_packet(std::move(pkt));
    }
}

static void tcp_accept_loop() {
    ::listen(g_tcp_listen, SOMAXCONN);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_storage peer{};
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

} // anonymous namespace

void init(const Config &cfg) {
    g_cfg = cfg;
    if (g_cfg.node_id_path.empty()) {
        g_cfg.node_id_path = DEFAULT_NODE_ID_FILE;
    }

    if (g_cfg.node_id == 0) {
        std::ifstream in{g_cfg.node_id_path};
        if (in)
            in >> g_cfg.node_id;
    }

    g_udp_sock = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (g_udp_sock < 0)
        throw std::system_error(errno, std::generic_category(), "UDP socket");
    int off = 0;
    ::setsockopt(g_udp_sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(cfg.port);
    addr6.sin6_addr = in6addr_any;
    if (::bind(g_udp_sock, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0)
        throw std::system_error(errno, std::generic_category(), "UDP bind");

    g_tcp_listen = ::socket(AF_INET6, SOCK_STREAM, 0);
    if (g_tcp_listen < 0)
        throw std::system_error(errno, std::generic_category(), "TCP socket");
    ::setsockopt(g_tcp_listen, SOL_SOCKET, SO_REUSEADDR, &off, sizeof(off));
    ::setsockopt(g_tcp_listen, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    if (::bind(g_tcp_listen, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0)
        throw std::system_error(errno, std::generic_category(), "TCP bind");

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

    std::lock_guard lock{g_mutex};
    g_queue.clear();

    std::lock_guard rlock{g_remotes_mutex};
    for (auto &[_, rem] : g_remotes) {
        if (rem.proto == Protocol::TCP && rem.tcp_fd >= 0) {
            ::close(rem.tcp_fd);
        }
    }
    g_remotes.clear();
    g_callback = nullptr;
}

void add_remote(node_t node, const std::string &host, uint16_t port, Protocol proto) {
    Remote rem{};
    rem.proto = proto;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (proto == Protocol::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    char port_str[16];
    std::snprintf(port_str, sizeof(port_str), "%u", port);
    addrinfo *res = nullptr;
    if (::getaddrinfo(host.c_str(), port_str, &hints, &res) != 0) {
        throw std::invalid_argument("invalid host address");
    }

    for (auto *p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET || p->ai_family == AF_INET6) {
            rem.addr_len = static_cast<socklen_t>(p->ai_addrlen);
            std::memcpy(&rem.addr, p->ai_addr, p->ai_addrlen);
            break;
        }
    }
    ::freeaddrinfo(res);
    if (rem.addr_len == 0)
        throw std::invalid_argument("host address resolution failed");

    if (proto == Protocol::TCP) {
        reconnect_tcp(rem);
    }

    std::lock_guard lock{g_remotes_mutex};
    g_remotes[node] = rem;
}

void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

node_t local_node() noexcept {
    if (g_cfg.node_id != 0)
        return g_cfg.node_id;

    std::ifstream in{g_cfg.node_id_path};
    if (in) {
        in >> g_cfg.node_id;
        if (g_cfg.node_id != 0)
            return g_cfg.node_id;
    }

    ifaddrs *ifa = nullptr;
    if (::getifaddrs(&ifa) == 0) {
        for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
            if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK))
                continue;

            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_PACKET) {
                auto *ll = reinterpret_cast<sockaddr_ll *>(cur->ifa_addr);
                std::size_t val = 0;
                for (int i = 0; i < ll->sll_halen; ++i)
                    val = val * 131 + ll->sll_addr[i];
                ::freeifaddrs(ifa);
                g_cfg.node_id = static_cast<node_t>(val & 0x7fffffff);
                std::filesystem::create_directories(
                    std::filesystem::path(g_cfg.node_id_path).parent_path());
                std::ofstream out{g_cfg.node_id_path};
                out << g_cfg.node_id;
                return g_cfg.node_id;
            }

            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(cur->ifa_addr);
                const auto *b = reinterpret_cast<const uint8_t *>(&sin->sin_addr);
                std::size_t val = 0;
                for (int i = 0; i < 4; ++i)
                    val = val * 131 + b[i];
                ::freeifaddrs(ifa);
                g_cfg.node_id = static_cast<node_t>(val & 0x7fffffff);
                std::filesystem::create_directories(
                    std::filesystem::path(g_cfg.node_id_path).parent_path());
                std::ofstream out{g_cfg.node_id_path};
                out << g_cfg.node_id;
                return g_cfg.node_id;
            }
        }
        ::freeifaddrs(ifa);
    }

    char host[256]{};
    if (::gethostname(host, sizeof(host)) == 0) {
        g_cfg.node_id = static_cast<node_t>(std::hash<std::string_view>{}(host) & 0x7fffffff);
        std::filesystem::create_directories(
            std::filesystem::path(g_cfg.node_id_path).parent_path());
        std::ofstream out{g_cfg.node_id_path};
        out << g_cfg.node_id;
        return g_cfg.node_id;
    }

    return 1;
}

std::errc send(node_t node, std::span<const std::byte> data) {
    Remote rem;
    {
        std::lock_guard lock{g_remotes_mutex};
        auto it = g_remotes.find(node);
        if (it == g_remotes.end())
            return std::errc::host_unreachable;
        rem = it->second;
    }

    auto buf = frame_payload(data);

    if (rem.proto == Protocol::TCP) {
        int fd = rem.tcp_fd;
        bool transient = fd < 0;

        if (transient) {
            fd = ::socket(rem.addr.ss_family, SOCK_STREAM, 0);
            if (fd < 0 ||
                ::connect(fd, reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len) != 0) {
                if (fd >= 0)
                    ::close(fd);
                return std::errc::connection_refused;
            }
        }

        std::size_t sent = 0;
        while (sent < buf.size()) {
            ssize_t n = ::send(fd, buf.data() + sent, buf.size() - sent, 0);
            if (n < 0) {
                if (transient)
                    ::close(fd);
                return std::errc::io_error;
            }
            sent += static_cast<std::size_t>(n);
        }

        if (transient)
            ::close(fd);
        return std::errc{};
    }

    ssize_t n = ::sendto(g_udp_sock, buf.data(), buf.size(), 0,
                         reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len);
    if (n < 0 || static_cast<std::size_t>(n) != buf.size()) {
        return std::errc::io_error;
    }
    return std::errc{};
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

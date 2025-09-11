// Add connection state management
enum class ConnectionState { Disconnected, Connecting, Connected, Failed, Reconnecting };

struct Remote {
    sockaddr_in addr{};
    Protocol proto = Protocol::UDP;
    int tcp_fd = -1;
    ConnectionState state = ConnectionState::Disconnected;
    std::chrono::steady_clock::time_point last_attempt{};
    int retry_count = 0;
};
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

constexpr char NODE_ID_FILE[] = "/etc/xinim/node_id";

Driver driver{};

Driver::~Driver() { shutdown(); }

[[nodiscard]] bool Driver::connection_lost(int err) noexcept {
    return err == EPIPE || err == ECONNRESET || err == ENOTCONN || err == ECONNABORTED;
}

void Driver::reconnect_tcp(Remote &rem) {
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

[[nodiscard]] std::vector<std::byte> Driver::frame_payload(std::span<const std::byte> data) {
    node_t nid = local_node();
    std::vector<std::byte> buf(sizeof(nid) + data.size());
    std::memcpy(buf.data(), &nid, sizeof(nid));
    std::memcpy(buf.data() + sizeof(nid), data.data(), data.size());
    return buf;
}

void Driver::enqueue_packet(Packet &&pkt) {
    std::lock_guard lock{mutex_};
    if (cfg_.max_queue_length > 0 && queue_.size() >= cfg_.max_queue_length) {
        if (cfg_.overflow == OverflowPolicy::DropNewest)
            return;
        queue_.pop_front();
    }
    queue_.push_back(std::move(pkt));
    if (callback_)
        callback_(queue_.back());
}

void Driver::udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (running_.load(std::memory_order_relaxed)) {
        sockaddr_storage peer{};
        socklen_t len = sizeof(peer);
        ssize_t n = ::recvfrom(udp_sock_, buf.data(), buf.size(), 0,
                               reinterpret_cast<sockaddr *>(&peer), &len);
        if (n <= static_cast<ssize_t>(sizeof(node_t)))
            continue;

        Packet pkt{};
        std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
        pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
        enqueue_packet(std::move(pkt));
    }
}

void Driver::tcp_accept_loop() {
    ::listen(tcp_listen_, SOMAXCONN);
    while (running_.load(std::memory_order_relaxed)) {
        sockaddr_storage peer{};
        socklen_t len = sizeof(peer);
        int client = ::accept(tcp_listen_, reinterpret_cast<sockaddr *>(&peer), &len);
        if (client < 0)
            continue;

        std::array<std::byte, 2048> buf;
        while (true) {
            ssize_t n = ::recv(client, buf.data(), buf.size(), 0);
            if (n <= static_cast<ssize_t>(sizeof(node_t)))
                break;
            Packet pkt{};
            std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
            pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
            enqueue_packet(std::move(pkt));
        }
        ::close(client);
    }
}

void Driver::init(const Config &cfg) {
    cfg_ = cfg;

    if (cfg_.node_id == 0) {
        std::ifstream in{NODE_ID_FILE};
        if (in)
            in >> cfg_.node_id;
    }

    udp_sock_ = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_sock_ < 0)
        throw std::system_error(errno, std::generic_category(), "UDP socket");
    int off = 0;
    ::setsockopt(udp_sock_, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(cfg.port);
    addr6.sin6_addr = in6addr_any;
    if (::bind(udp_sock_, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0)
        throw std::system_error(errno, std::generic_category(), "UDP bind");

    tcp_listen_ = ::socket(AF_INET6, SOCK_STREAM, 0);
    if (tcp_listen_ < 0)
        throw std::system_error(errno, std::generic_category(), "TCP socket");
    ::setsockopt(tcp_listen_, SOL_SOCKET, SO_REUSEADDR, &off, sizeof(off));
    ::setsockopt(tcp_listen_, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    if (::bind(tcp_listen_, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0)
        throw std::system_error(errno, std::generic_category(), "TCP bind");

    running_.store(true, std::memory_order_relaxed);
    udp_thread_ = std::jthread{[this] { udp_recv_loop(); }};
    tcp_thread_ = std::jthread{[this] { tcp_accept_loop(); }};
}

void Driver::shutdown() noexcept {
    running_.store(false, std::memory_order_relaxed);
    if (udp_sock_ != -1) {
        ::close(udp_sock_);
        udp_sock_ = -1;
    }
    if (tcp_listen_ != -1) {
        ::shutdown(tcp_listen_, SHUT_RDWR);
        ::close(tcp_listen_);
        tcp_listen_ = -1;
    }

    {
        std::lock_guard lock{mutex_};
        queue_.clear();
    }

    std::lock_guard rlock{remotes_mutex_};
    for (auto &[_, rem] : remotes_) {
        if (rem.proto == Protocol::TCP && rem.tcp_fd >= 0) {
            ::close(rem.tcp_fd);
        }
    }
    remotes_.clear();
    callback_ = nullptr;
}

void Driver::add_remote(node_t node, const std::string &host, uint16_t port, Protocol proto) {
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

    std::lock_guard lock{remotes_mutex_};
    remotes_[node] = rem;
}

void Driver::set_recv_callback(RecvCallback cb) { callback_ = std::move(cb); }

node_t Driver::local_node() noexcept {
    if (cfg_.node_id != 0)
        return cfg_.node_id;

    std::ifstream in{NODE_ID_FILE};
    if (in) {
        in >> cfg_.node_id;
        if (cfg_.node_id != 0)
            return cfg_.node_id;
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
                cfg_.node_id = static_cast<node_t>(val & 0x7fffffff);
                std::filesystem::create_directories("/etc/xinim");
                std::ofstream out{NODE_ID_FILE};
                out << cfg_.node_id;
                return cfg_.node_id;
            }

            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(cur->ifa_addr);
                const auto *b = reinterpret_cast<const uint8_t *>(&sin->sin_addr);
                std::size_t val = 0;
                for (int i = 0; i < 4; ++i)
                    val = val * 131 + b[i];
                ::freeifaddrs(ifa);
                cfg_.node_id = static_cast<node_t>(val & 0x7fffffff);
                std::filesystem::create_directories("/etc/xinim");
                std::ofstream out{NODE_ID_FILE};
                out << cfg_.node_id;
                return cfg_.node_id;
            }
        }
        ::freeifaddrs(ifa);
    }

    char host[256]{};
    if (::gethostname(host, sizeof(host)) == 0) {
        cfg_.node_id = static_cast<node_t>(std::hash<std::string_view>{}(host) & 0x7fffffff);
        std::filesystem::create_directories("/etc/xinim");
        std::ofstream out{NODE_ID_FILE};
        out << cfg_.node_id;
        return cfg_.node_id;
    }

    return 1;
}

std::errc Driver::send(node_t node, std::span<const std::byte> data) {
    Remote rem;
    {
        std::lock_guard lock{remotes_mutex_};
        auto it = remotes_.find(node);
        if (it == remotes_.end())
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

    ssize_t n = ::sendto(udp_sock_, buf.data(), buf.size(), 0,
                         reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len);
    if (n < 0 || static_cast<std::size_t>(n) != buf.size()) {
        return std::errc::io_error;
    }
    return std::errc{};
}

bool Driver::recv(Packet &out) {
    std::lock_guard lock{mutex_};
    if (queue_.empty())
        return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void Driver::reset() noexcept {
    std::lock_guard lock{mutex_};
    queue_.clear();
}

} // namespace net

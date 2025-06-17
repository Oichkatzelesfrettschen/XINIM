/**
 * @file net_driver.cpp
 * @brief Robust UDP/TCP network driver for Lattice IPC.
 *
 * This implementation merges:
 *  - Configurable receive queue length & overflow policy
 *  - Hybrid UDP/TCP transport (transient UDP or persistent TCP)
 *  - Framed packets prefixed with local_node()
 *  - Background I/O threads for UDP datagrams and TCP accepts
 *  - Optional receive callback on packet arrival
 *  - Blocking send/recv APIs
 *  - Auto-detectable or configured node identifier
 */

#include "net_driver.hpp"

#if defined(_WIN32)
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__DragonFly__)
#include <net/if_dl.h>
#else
#include <netpacket/packet.h>
#endif
#include <sys/socket.h>
#include <unistd.h>
#endif

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

/** Active driver configuration. */
static Config g_cfg{};
/** Path storing persistent node identifier. */
static constexpr char NODE_ID_FILE[] = "/etc/xinim/node_id";
/** UDP socket descriptor. */
static int g_udp_sock = -1;
/** TCP listening socket descriptor. */
static int g_tcp_listen = -1;

/**
 * Represents a remote peer: address, transport, and optional persistent TCP fd.
 */
struct Remote {
    sockaddr_storage addr{}; ///< Destination IPv4/IPv6 address
    socklen_t addr_len{};    ///< Length of addr
    Protocol proto;          ///< UDP or TCP
    int tcp_fd = -1;         ///< persistent TCP socket if proto==TCP
};

/** Map from node ID to remote peer info. */
static std::unordered_map<node_t, Remote> g_remotes;
/** Mutex guarding g_remotes. */
static std::mutex g_remotes_mutex;
/** Queue of received packets. */
static std::deque<Packet> g_queue;
/** Mutex guarding g_queue. */
static std::mutex g_mutex;
/** Optional callback invoked on packet arrival. */
static RecvCallback g_callback;
/** Controls background I/O threads. */
static std::atomic<bool> g_running{false};
/** Background receiver threads. */
static std::jthread g_udp_thread, g_tcp_thread;

/**
 * @brief Platform-specific helper deriving a node identifier.
 *
 * Enumerates network interfaces using APIs tailored for each operating system
 * and hashes the first active, non-loopback address. The function returns
 * ``0`` when no suitable interface can be found.
 */
[[nodiscard]] static node_t derive_node() noexcept {
#if defined(_WIN32)
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) {
        return 0;
    }
    std::vector<unsigned char> buf(size);
    auto *addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addrs, &size) != NO_ERROR) {
        return 0;
    }
    for (auto *cur = addrs; cur != nullptr; cur = cur->Next) {
        if (cur->OperStatus != IfOperStatusUp || cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        if (cur->PhysicalAddressLength > 0) {
            std::size_t value = 0;
            for (unsigned i = 0; i < cur->PhysicalAddressLength; ++i) {
                value = value * 131 + cur->PhysicalAddress[i];
            }
            return static_cast<node_t>(value & 0x7fffffff);
        }
        for (auto *ua = cur->FirstUnicastAddress; ua; ua = ua->Next) {
            auto fam = ua->Address.lpSockaddr->sa_family;
            if (fam == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(ua->Address.lpSockaddr);
                std::size_t value = 0;
                const auto *b = reinterpret_cast<const unsigned char *>(&sin->sin_addr);
                for (unsigned i = 0; i < sizeof(sin->sin_addr); ++i) {
                    value = value * 131 + b[i];
                }
                return static_cast<node_t>(value & 0x7fffffff);
            }
            if (fam == AF_INET6) {
                auto *sin6 = reinterpret_cast<sockaddr_in6 *>(ua->Address.lpSockaddr);
                std::size_t value = 0;
                const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
                for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                    value = value * 131 + b[i];
                }
                return static_cast<node_t>(value & 0x7fffffff);
            }
        }
    }
    return 0;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__DragonFly__)
    ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        return 0;
    }
    node_t result = 0;
    for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
        if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }
        if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_LINK) {
            auto *sdl = reinterpret_cast<sockaddr_dl *>(cur->ifa_addr);
            if (sdl->sdl_alen > 0) {
                std::size_t value = 0;
                const auto *b = reinterpret_cast<const unsigned char *>(LLADDR(sdl));
                for (int i = 0; i < sdl->sdl_alen; ++i) {
                    value = value * 131 + b[i];
                }
                result = static_cast<node_t>(value & 0x7fffffff);
                break;
            }
        }
        if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET) {
            auto *sin = reinterpret_cast<sockaddr_in *>(cur->ifa_addr);
            std::size_t value = 0;
            const auto *b = reinterpret_cast<const unsigned char *>(&sin->sin_addr);
            for (unsigned i = 0; i < sizeof(sin->sin_addr); ++i) {
                value = value * 131 + b[i];
            }
            result = static_cast<node_t>(value & 0x7fffffff);
            break;
        }
        if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET6) {
            auto *sin6 = reinterpret_cast<sockaddr_in6 *>(cur->ifa_addr);
            std::size_t value = 0;
            const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
            for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                value = value * 131 + b[i];
            }
            result = static_cast<node_t>(value & 0x7fffffff);
            break;
        }
    }
    freeifaddrs(ifa);
    return result;
#else
    ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        return 0;
    }
    node_t result = 0;
    for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
        if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }
        if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_PACKET) {
            auto *ll = reinterpret_cast<sockaddr_ll *>(cur->ifa_addr);
            std::size_t value = 0;
            for (int i = 0; i < ll->sll_halen; ++i) {
                value = value * 131 + ll->sll_addr[i];
            }
            result = static_cast<node_t>(value & 0x7fffffff);
            break;
        }
        if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET) {
            auto *sin = reinterpret_cast<sockaddr_in *>(cur->ifa_addr);
            std::size_t value = 0;
            const auto *b = reinterpret_cast<const unsigned char *>(&sin->sin_addr);
            for (unsigned i = 0; i < sizeof(sin->sin_addr); ++i) {
                value = value * 131 + b[i];
            }
            result = static_cast<node_t>(value & 0x7fffffff);
            break;
        }
        if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET6) {
            auto *sin6 = reinterpret_cast<sockaddr_in6 *>(cur->ifa_addr);
            std::size_t value = 0;
            const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
            for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                value = value * 131 + b[i];
            }
            result = static_cast<node_t>(value & 0x7fffffff);
            break;
        }
    }
    freeifaddrs(ifa);
    return result;
#endif
}

/**
 * @brief Frame a payload by prefixing with the local node ID.
 */
static std::vector<std::byte> frame_payload(std::span<const std::byte> data) {
    node_t nid = local_node();
    std::vector<std::byte> buf(sizeof(nid) + data.size());
    std::memcpy(buf.data(), &nid, sizeof(nid));
    std::memcpy(buf.data() + sizeof(nid), data.data(), data.size());
    return buf;
}

/**
 * @brief Enqueue a packet, applying overflow policy if the queue is full.
 */
static void enqueue_packet(Packet &&pkt) {
    std::lock_guard lock{g_mutex};
    if (g_cfg.max_queue_length > 0 &&
        g_queue.size() >= static_cast<size_t>(g_cfg.max_queue_length)) {
        if (g_cfg.overflow == OverflowPolicy::DropOldest) {
            g_queue.pop_front();
        } else if (g_cfg.overflow == OverflowPolicy::DropNewest) {
            return; // drop the new packet
        }
    }
    g_queue.push_back(std::move(pkt));
    if (g_callback) {
        g_callback(g_queue.back());
    }
}

/**
 * @brief Background loop receiving UDP datagrams.
 */
static void udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_storage peer{};
        socklen_t len = sizeof(peer);
        ssize_t n = ::recvfrom(g_udp_sock, buf.data(), buf.size(), 0,
                               reinterpret_cast<sockaddr *>(&peer), &len);
        if (n <= static_cast<ssize_t>(sizeof(node_t))) {
            continue;
        }
        Packet pkt;
        std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
        pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
        enqueue_packet(std::move(pkt));
    }
}

/**
 * @brief Background loop accepting and processing TCP connections.
 */
static void tcp_accept_loop() {
    ::listen(g_tcp_listen, SOMAXCONN);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_storage peer{};
        socklen_t len = sizeof(peer);
        int client = ::accept(g_tcp_listen, reinterpret_cast<sockaddr *>(&peer), &len);
        if (client < 0) {
            continue;
        }
        std::array<std::byte, 2048> buf;
        while (true) {
            ssize_t n = ::recv(client, buf.data(), buf.size(), 0);
            if (n <= static_cast<ssize_t>(sizeof(node_t))) {
                break;
            }
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
    if (g_cfg.node_id == 0) {
        std::ifstream idin{NODE_ID_FILE};
        if (idin) {
            node_t file_node_id = 0;
            if (idin >> file_node_id) {
                g_cfg.node_id = file_node_id;
            }
        }
    }

    // Create UDP socket (dual stack IPv6/IPv4)
    g_udp_sock = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (g_udp_sock < 0) {
        throw std::system_error(errno, std::generic_category(), "net_driver: UDP socket");
    }
    int off = 0;
    ::setsockopt(g_udp_sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(cfg.port);
    addr6.sin6_addr = in6addr_any;
    if (::bind(g_udp_sock, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0) {
        throw std::system_error(errno, std::generic_category(), "net_driver: UDP bind");
    }

    // Create TCP listening socket (dual stack)
    g_tcp_listen = ::socket(AF_INET6, SOCK_STREAM, 0);
    if (g_tcp_listen < 0) {
        throw std::system_error(errno, std::generic_category(), "net_driver: TCP socket");
    }
    int opt = 1;
    ::setsockopt(g_tcp_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(g_tcp_listen, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    if (::bind(g_tcp_listen, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0) {
        throw std::system_error(errno, std::generic_category(), "net_driver: TCP bind");
    }

    // Launch background I/O threads
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
    {
        std::lock_guard lock{g_remotes_mutex};
        for (auto &[_, rem] : g_remotes) {
            if (rem.proto == Protocol::TCP && rem.tcp_fd >= 0) {
                ::close(rem.tcp_fd);
            }
        }
        g_remotes.clear();
    }
    g_callback = nullptr;
}

void add_remote(node_t node, const std::string &host, uint16_t port, Protocol proto) {
    Remote rem{};
    rem.proto = proto;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (proto == Protocol::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    char port_str[16]{};
    std::snprintf(port_str, sizeof(port_str), "%u", port);
    addrinfo *res = nullptr;
    if (::getaddrinfo(host.c_str(), port_str, &hints, &res) != 0) {
        throw std::invalid_argument("net_driver: invalid host address");
    }
    for (auto *p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET || p->ai_family == AF_INET6) {
            rem.addr_len = static_cast<socklen_t>(p->ai_addrlen);
            std::memcpy(&rem.addr, p->ai_addr, p->ai_addrlen);
            break;
        }
    }
    ::freeaddrinfo(res);
    if (rem.addr_len == 0) {
        throw std::invalid_argument("net_driver: invalid host address");
    }

    if (proto == Protocol::TCP) {
        rem.tcp_fd = ::socket(rem.addr.ss_family, SOCK_STREAM, 0);
        if (rem.tcp_fd < 0 ||
            ::connect(rem.tcp_fd, reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len) != 0) {
            if (rem.tcp_fd >= 0) {
                ::close(rem.tcp_fd);
            }
            throw std::system_error(errno, std::generic_category(), "net_driver: TCP connect");
        }
    }
    {
        std::lock_guard lock{g_remotes_mutex};
        g_remotes[node] = rem;
    }
}

void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

node_t local_node() noexcept {
    // 1) user-supplied or loaded identifier
    if (g_cfg.node_id != 0) {
        return g_cfg.node_id;
    }
    {
        std::ifstream idin{NODE_ID_FILE};
        if (idin) {
            idin >> g_cfg.node_id;
            if (g_cfg.node_id != 0) {
                return g_cfg.node_id;
            }
        }
    }
    // 2) derive from active network interface
    node_t derived = derive_node();
    if (derived != 0) {
        g_cfg.node_id = derived;
        std::filesystem::create_directories("/etc/xinim");
        std::ofstream out{NODE_ID_FILE, std::ios::trunc};
        if (out) {
            out << derived;
        }
        return derived;
    }
    // 3) fallback to host name
    char host[256]{};
    if (::gethostname(host, sizeof(host)) == 0) {
        auto h = std::hash<std::string_view>{}(host) & 0x7fffffff;
        g_cfg.node_id = static_cast<node_t>(h);
        std::filesystem::create_directories("/etc/xinim");
        std::ofstream out{NODE_ID_FILE, std::ios::trunc};
        if (out) {
            out << g_cfg.node_id;
        }
        return g_cfg.node_id;
    }
    return 0;
}

/**
 * @brief Transmit a framed payload to a remote node.
 *
 * @param node Identifier of the destination peer.
 * @param data Raw payload bytes excluding the local node prefix.
 * @return ``std::errc::success`` on success or a descriptive ``std::errc``
 *         when a logical error occurs. Socket failures throw
 *         ``std::system_error``.
 */
std::errc send(node_t node, std::span<const std::byte> data) {
    Remote rem;
    {
        std::lock_guard lock{g_remotes_mutex};
        auto it = g_remotes.find(node);
        if (it == g_remotes.end()) {
            return std::errc::host_unreachable; // unknown destination
        }
        rem = it->second;
    }
    auto buf = frame_payload(data);

    if (rem.proto == Protocol::TCP) {
        int fd = rem.tcp_fd;
        bool tmp = false;
        if (fd < 0) {
            fd = ::socket(rem.addr.ss_family, SOCK_STREAM, 0);
            if (fd < 0) {
                throw std::system_error(errno, std::generic_category(), "net_driver: socket");
            }
            if (::connect(fd, reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len) != 0) {
                int err = errno;
                ::close(fd);
                throw std::system_error(err, std::generic_category(), "net_driver: connect");
            }
            tmp = true;
        }
        std::size_t sent = 0;
        while (sent < buf.size()) {
            ssize_t n = ::send(fd, buf.data() + sent, buf.size() - sent, 0);
            if (n < 0) {
                if (tmp) {
                    ::close(fd);
                }
                throw std::system_error(errno, std::generic_category(), "net_driver: send");
            }
            sent += static_cast<std::size_t>(n);
        }
        if (tmp) {
            ::close(fd);
        }
        return std::errc{};
    }

    if (g_udp_sock < 0) {
        return std::errc::not_connected;
    }
    ssize_t n = ::sendto(g_udp_sock, buf.data(), buf.size(), 0,
                         reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len);
    if (n < 0) {
        throw std::system_error(errno, std::generic_category(), "net_driver: sendto");
    }
    if (n != static_cast<ssize_t>(buf.size())) {
        return std::errc::no_buffer_space;
    }
    return std::errc{};
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

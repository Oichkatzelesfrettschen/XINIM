/**
 * @file net_driver.cpp
 * @brief Robust UDP/TCP networking backend for Lattice IPC (IPv4/IPv6, C++23).
 * @warning Lacks zero-copy optimizations; consider io_uring for future refactor.
 */

#include "net_driver.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <cerrno>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using SOCKET = int;
constexpr int INVALID_SOCKET = -1;
inline int closesocket(SOCKET fd) { return close(fd); }
inline int WSAGetLastError() { return errno; }
struct WSADATA {};
inline int WSAStartup(unsigned short, WSADATA*) { return 0; }
inline void WSACleanup() {}
constexpr int WSAECONNRESET = ECONNRESET;
constexpr int WSAENOTCONN = ENOTCONN;
constexpr int WSAECONNABORTED = ECONNABORTED;
constexpr int SD_BOTH = SHUT_RDWR;
#define MAKEWORD(a, b) static_cast<unsigned short>(((a) & 0xff) | (((b) & 0xff) << 8))
#endif

#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net {
namespace {

static WSADATA g_wsa_data{};
static bool g_wsa_initialized = false;

static Config g_cfg{};
static SOCKET g_udp_sock = INVALID_SOCKET;
static SOCKET g_tcp_listen = INVALID_SOCKET;

/**
 * @brief Runtime metadata for a registered remote peer.
 */
struct Remote {
    sockaddr_in addr{}; ///< Peer socket address.
    socklen_t addr_len = static_cast<socklen_t>(sizeof(sockaddr_in)); ///< Address length.
    Protocol proto = Protocol::UDP; ///< Transport protocol.
    SOCKET tcp_fd = INVALID_SOCKET; ///< Connected TCP socket for persistent links.
};

static std::unordered_map<node_t, Remote> g_remotes;
static std::mutex g_remotes_mutex;

static std::deque<Packet> g_queue;
static std::mutex g_mutex;
static RecvCallback g_callback;
static std::atomic<bool> g_running{false};
static std::thread g_udp_thread, g_tcp_thread;  // Use std::thread instead of jthread for compatibility

/**
 * @brief Resolve the persisted node ID path based on platform conventions.
 *
 * Honors Config::node_id_dir when provided; otherwise prefers APPDATA/
 * LOCALAPPDATA on Windows and XDG_STATE_HOME or HOME on POSIX.
 *
 * @return Full filesystem path to the node_id file.
 */
[[nodiscard]] static std::filesystem::path node_id_file() {
    if (!g_cfg.node_id_dir.empty()) {
        return g_cfg.node_id_dir / "node_id";
    }

#ifdef _WIN32
    char *app_data = nullptr;
    size_t len = 0;
    if (_dupenv_s(&app_data, &len, "APPDATA") == 0 && app_data) {
        auto path = std::filesystem::path{app_data} / "xinim" / "node_id";
        std::free(app_data);
        return path;
    }
    if (_dupenv_s(&app_data, &len, "LOCALAPPDATA") == 0 && app_data) {
        auto path = std::filesystem::path{app_data} / "xinim" / "node_id";
        std::free(app_data);
        return path;
    }
    return std::filesystem::path{"node_id"};
#else
    if (const char *state_home = std::getenv("XDG_STATE_HOME"); state_home && *state_home) {
        return std::filesystem::path{state_home} / "xinim" / "node_id";
    }
    if (const char *data_home = std::getenv("XDG_DATA_HOME"); data_home && *data_home) {
        return std::filesystem::path{data_home} / "xinim" / "node_id";
    }
    if (const char *home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path{home} / ".local" / "state" / "xinim" / "node_id";
    }
    return std::filesystem::path{"node_id"};
#endif
}

/**
 * @brief Check whether the provided socket error indicates a lost connection.
 */
[[nodiscard]] static bool connection_lost(int err) noexcept {
    return err == WSAECONNRESET || err == WSAENOTCONN || err == WSAECONNABORTED;
}

/**
 * @brief Re-establish the TCP connection for a remote peer.
 *
 * @param rem Remote peer metadata updated with the connected socket.
 */
static void reconnect_tcp(Remote &rem) {
    if (rem.tcp_fd != INVALID_SOCKET) closesocket(rem.tcp_fd);
    rem.tcp_fd = socket(rem.addr.sin_family, SOCK_STREAM, 0);
    if (rem.tcp_fd == INVALID_SOCKET) throw std::system_error(WSAGetLastError(), std::system_category(), "TCP socket");
    if (connect(rem.tcp_fd, reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len) != 0) {
        int err = WSAGetLastError();
        closesocket(rem.tcp_fd);
        rem.tcp_fd = INVALID_SOCKET;
        throw std::system_error(err, std::system_category(), "TCP connect");
    }
}

/**
 * @brief Prepend the local node ID to the outgoing payload.
 *
 * @param data Payload bytes.
 * @return Framed payload containing [node_id | data].
 */
[[nodiscard]] static std::vector<std::byte> frame_payload(std::span<const std::byte> data) {
    node_t nid = local_node();
    std::vector<std::byte> buf(sizeof(nid) + data.size());
    std::memcpy(buf.data(), &nid, sizeof(nid));
    std::memcpy(buf.data() + sizeof(nid), data.data(), data.size());
    return buf;
}

/**
 * @brief Enqueue a received packet and invoke the callback if present.
 *
 * Applies the configured overflow policy when the queue is full.
 *
 * @param pkt Packet to enqueue.
 */
static void enqueue_packet(Packet &&pkt) {
    std::lock_guard lock{g_mutex};
    if (g_cfg.max_queue_length > 0 && g_queue.size() >= g_cfg.max_queue_length) {
        if (g_cfg.overflow == OverflowPolicy::DropNewest) return;
        g_queue.pop_front();
    }
    g_queue.push_back(std::move(pkt));
    if (g_callback) g_callback(g_queue.back());
}

/**
 * @brief Receive UDP packets and push them into the shared queue.
 */
static void udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = static_cast<socklen_t>(sizeof(peer));
#ifdef _WIN32
        const int buf_len = static_cast<int>(buf.size());
        const auto n = ::recvfrom(g_udp_sock, reinterpret_cast<char *>(buf.data()), buf_len, 0,
                                  reinterpret_cast<sockaddr *>(&peer), &len);
#else
        const auto n = ::recvfrom(g_udp_sock, reinterpret_cast<char *>(buf.data()), buf.size(), 0,
                                  reinterpret_cast<sockaddr *>(&peer), &len);
#endif
        if (n <= 0) continue;
        const auto n_bytes = static_cast<std::size_t>(n);
        if (n_bytes <= sizeof(node_t)) continue;
        Packet pkt;
        std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
        pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n_bytes);
        enqueue_packet(std::move(pkt));
    }
}

/**
 * @brief Accept TCP clients and enqueue received packets.
 */
static void tcp_accept_loop() {
    listen(g_tcp_listen, SOMAXCONN);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        socklen_t len = static_cast<socklen_t>(sizeof(peer));
        SOCKET client = ::accept(g_tcp_listen, reinterpret_cast<sockaddr *>(&peer), &len);
        if (client == INVALID_SOCKET) continue;
        std::array<std::byte, 2048> buf;
        while (true) {
            const auto n =
#ifdef _WIN32
                ::recv(client, reinterpret_cast<char *>(buf.data()), static_cast<int>(buf.size()), 0);
#else
                ::recv(client, reinterpret_cast<char *>(buf.data()), buf.size(), 0);
#endif
            if (n <= 0) break;
            const auto n_bytes = static_cast<std::size_t>(n);
            if (n_bytes <= sizeof(node_t)) break;
            Packet pkt;
            std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
            pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n_bytes);
            enqueue_packet(std::move(pkt));
        }
        closesocket(client);
    }
}

} // anonymous namespace

void init(const Config &cfg) {
    g_cfg = cfg;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &g_wsa_data) != 0) {
        throw std::system_error(WSAGetLastError(), std::system_category(), "WSAStartup");
    }
    g_wsa_initialized = true;

    if (g_cfg.node_id == 0) {
        std::ifstream in{node_id_file()};
        if (in) in >> g_cfg.node_id;
    }

    g_udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_udp_sock == INVALID_SOCKET) throw std::system_error(WSAGetLastError(), std::system_category(), "UDP socket");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(g_udp_sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        throw std::system_error(WSAGetLastError(), std::system_category(), "UDP bind");

    g_tcp_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (g_tcp_listen == INVALID_SOCKET) throw std::system_error(WSAGetLastError(), std::system_category(), "TCP socket");
    int opt = 1;
    setsockopt(g_tcp_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt), sizeof(opt));
    if (bind(g_tcp_listen, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        throw std::system_error(WSAGetLastError(), std::system_category(), "TCP bind");

    g_running.store(true, std::memory_order_relaxed);
    g_udp_thread = std::thread{udp_recv_loop};
    g_tcp_thread = std::thread{tcp_accept_loop};
}

void shutdown() noexcept {
    g_running.store(false, std::memory_order_relaxed);
    if (g_udp_sock != INVALID_SOCKET) {
        closesocket(g_udp_sock);
        g_udp_sock = INVALID_SOCKET;
    }
    if (g_tcp_listen != INVALID_SOCKET) {
        ::shutdown(g_tcp_listen, SD_BOTH);
        closesocket(g_tcp_listen);
        g_tcp_listen = INVALID_SOCKET;
    }
    if (g_udp_thread.joinable()) g_udp_thread.join();
    if (g_tcp_thread.joinable()) g_tcp_thread.join();
    {
        std::lock_guard lock{g_mutex};
        g_queue.clear();
    }
    {
        std::lock_guard rlock{g_remotes_mutex};
        for (auto &[_, rem] : g_remotes)
            if (rem.proto == Protocol::TCP && rem.tcp_fd != INVALID_SOCKET) closesocket(rem.tcp_fd);
        g_remotes.clear();
    }
    g_callback = nullptr;
    if (g_wsa_initialized) {
        WSACleanup();
        g_wsa_initialized = false;
    }
}

void add_remote(node_t node, const std::string &host, uint16_t port, Protocol proto) {
    Remote rem{};
    rem.proto = proto;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = (proto == Protocol::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

    char port_str[16];
    std::snprintf(port_str, sizeof(port_str), "%u", port);
    addrinfo *res = nullptr;
    if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0)
        throw std::invalid_argument("invalid host address");

    for (auto *p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            rem.addr_len = static_cast<socklen_t>(p->ai_addrlen);
            std::memcpy(&rem.addr, p->ai_addr, p->ai_addrlen);
            break;
        }
    }
    freeaddrinfo(res);
    if (rem.addr_len == 0) throw std::invalid_argument("host address resolution failed");

    if (proto == Protocol::TCP) reconnect_tcp(rem);

    std::lock_guard lock{g_remotes_mutex};
    g_remotes[node] = rem;
}

void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

node_t local_node() noexcept {
    if (g_cfg.node_id != 0) return g_cfg.node_id;
    std::ifstream in{node_id_file()};
    if (in && (in >> g_cfg.node_id) && g_cfg.node_id != 0) return g_cfg.node_id;

    // Simplified for Windows: use hostname hash
    char host[256]{};
    if (gethostname(host, sizeof(host)) == 0) {
        g_cfg.node_id = static_cast<node_t>(std::hash<std::string_view>{}(host) & 0x7fffffff);
        std::filesystem::create_directories(node_id_file().parent_path());
        std::ofstream{node_id_file()} << g_cfg.node_id;
        return g_cfg.node_id;
    }

    return 1;
}

std::errc send(node_t node, std::span<const std::byte> data) {
    Remote rem;
    {
        std::lock_guard lock{g_remotes_mutex};
        auto it = g_remotes.find(node);
        if (it == g_remotes.end()) return std::errc::host_unreachable;
        rem = it->second;
    }

    auto buf = frame_payload(data);

    if (rem.proto == Protocol::TCP) {
        SOCKET fd = rem.tcp_fd;
        bool transient = fd == INVALID_SOCKET;
        int err = 0;

        auto try_send = [&](SOCKET sock) -> std::errc {
            size_t sent = 0;
            while (sent < buf.size()) {
#ifdef _WIN32
                const int chunk = static_cast<int>(buf.size() - sent);
                const auto n = ::send(sock, reinterpret_cast<const char *>(buf.data() + sent), chunk, 0);
#else
                const auto n = ::send(sock, reinterpret_cast<const char *>(buf.data() + sent), buf.size() - sent, 0);
#endif
                if (n <= 0) {
                    err = WSAGetLastError();
                    return std::errc::io_error;
                }
                sent += static_cast<size_t>(n);
            }
            return std::errc{};
        };

        if (transient) {
            fd = socket(rem.addr.sin_family, SOCK_STREAM, 0);
            if (fd == INVALID_SOCKET || connect(fd, reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len) != 0) {
                if (fd != INVALID_SOCKET) closesocket(fd);
                return std::errc::connection_refused;
            }
        }

        auto rc = try_send(fd);
        if (rc != std::errc{} && !transient && connection_lost(err) && rem.tcp_fd != INVALID_SOCKET) {
            try {
                reconnect_tcp(rem);
                {
                    std::lock_guard lock{g_remotes_mutex};
                    auto it = g_remotes.find(node);
                    if (it != g_remotes.end()) it->second.tcp_fd = rem.tcp_fd;
                }
                rc = try_send(rem.tcp_fd);
            } catch (...) {
                rc = std::errc::io_error;
            }
        }

        if (transient) closesocket(fd);
        return rc;
    }

    const auto n =
#ifdef _WIN32
        ::sendto(g_udp_sock, reinterpret_cast<const char *>(buf.data()), static_cast<int>(buf.size()), 0,
                 reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len);
#else
        ::sendto(g_udp_sock, reinterpret_cast<const char *>(buf.data()), buf.size(), 0,
                 reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len);
#endif
    return (n < 0 || static_cast<size_t>(n) != buf.size()) ? std::errc::io_error : std::errc{};
}

bool recv(Packet &out) {
    std::lock_guard lock{g_mutex};
    if (g_queue.empty()) return false;
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

/**
 * @brief Clear the receive queue without affecting socket state.
 */
void reset() noexcept {
    std::lock_guard lock{g_mutex};
    g_queue.clear();
}

/**
 * @brief Simulate a socket failure by closing active descriptors.
 */
void simulate_socket_failure() noexcept {
    if (g_udp_sock != INVALID_SOCKET) {
        closesocket(g_udp_sock);
        g_udp_sock = INVALID_SOCKET;
    }
    if (g_tcp_listen != INVALID_SOCKET) {
        closesocket(g_tcp_listen);
        g_tcp_listen = INVALID_SOCKET;
    }
}

} // namespace net

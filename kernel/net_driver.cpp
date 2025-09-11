/**
 * @file net_driver.cpp
 * @brief Robust UDP/TCP networking backend for Lattice IPC (IPv4/IPv6, C++23).
 * @warning Lacks zero-copy optimizations; consider io_uring for future refactor.
 */

#include "net_driver.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdlib>
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

static WSADATA g_wsa_data{};
static bool g_wsa_initialized = false;

static Config g_cfg{};
static SOCKET g_udp_sock = INVALID_SOCKET;
static SOCKET g_tcp_listen = INVALID_SOCKET;

struct Remote {
    sockaddr_in addr{};
    int addr_len = sizeof(sockaddr_in);
    Protocol proto = Protocol::UDP;
    SOCKET tcp_fd = INVALID_SOCKET;
};

static std::unordered_map<node_t, Remote> g_remotes;
static std::mutex g_remotes_mutex;

static std::deque<Packet> g_queue;
static std::mutex g_mutex;
static RecvCallback g_callback;
static std::atomic<bool> g_running{false};
static std::thread g_udp_thread, g_tcp_thread;  // Use std::thread instead of jthread for compatibility

[[nodiscard]] static std::filesystem::path node_id_file() {
    // Assume node_id_dir is a std::filesystem::path in Config; if not, adjust accordingly
    if (!g_cfg.node_id_dir.empty()) return g_cfg.node_id_dir / "node_id";
    // On Windows, no direct equivalent to geteuid; assume not root or skip
    // Use APPDATA or LOCALAPPDATA
    char *app_data = nullptr;
    size_t len = 0;
    if (_dupenv_s(&app_data, &len, "APPDATA") == 0 && app_data) {
        auto path = std::filesystem::path{app_data} / "xinim" / "node_id";
        free(app_data);
        return path;
    }
    if (_dupenv_s(&app_data, &len, "LOCALAPPDATA") == 0 && app_data) {
        auto path = std::filesystem::path{app_data} / "xinim" / "node_id";
        free(app_data);
        return path;
    }
    return std::filesystem::path{"node_id"};
}

[[nodiscard]] static bool connection_lost(int err) noexcept {
    return err == WSAECONNRESET || err == WSAENOTCONN || err == WSAECONNABORTED;
}

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
        if (g_cfg.overflow == OverflowPolicy::DropNewest) return;
        g_queue.pop_front();
    }
    g_queue.push_back(std::move(pkt));
    if (g_callback) g_callback(g_queue.back());
}

static void udp_recv_loop() {
    std::array<std::byte, 2048> buf;
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        int len = sizeof(peer);
        int n = recvfrom(g_udp_sock, reinterpret_cast<char *>(buf.data()), buf.size(), 0,
                         reinterpret_cast<sockaddr *>(&peer), &len);
        if (n <= static_cast<int>(sizeof(node_t))) continue;
        Packet pkt;
        std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
        pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
        enqueue_packet(std::move(pkt));
    }
}

static void tcp_accept_loop() {
    listen(g_tcp_listen, SOMAXCONN);
    while (g_running.load(std::memory_order_relaxed)) {
        sockaddr_in peer{};
        int len = sizeof(peer);
        SOCKET client = accept(g_tcp_listen, reinterpret_cast<sockaddr *>(&peer), &len);
        if (client == INVALID_SOCKET) continue;
        std::array<std::byte, 2048> buf;
        while (true) {
            int n = recv(client, reinterpret_cast<char *>(buf.data()), buf.size(), 0);
            if (n <= static_cast<int>(sizeof(node_t))) break;
            Packet pkt;
            std::memcpy(&pkt.src_node, buf.data(), sizeof(pkt.src_node));
            pkt.payload.assign(buf.begin() + sizeof(pkt.src_node), buf.begin() + n);
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
        shutdown(g_tcp_listen, SD_BOTH);
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
            rem.addr_len = static_cast<int>(p->ai_addrlen);
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
                int n = ::send(sock, reinterpret_cast<const char *>(buf.data() + sent), buf.size() - sent, 0);
                if (n < 0) {
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

    int n = sendto(g_udp_sock, reinterpret_cast<const char *>(buf.data()), buf.size(), 0,
                   reinterpret_cast<sockaddr *>(&rem.addr), rem.addr_len);
    return (n < 0 || static_cast<size_t>(n) != buf.size()) ? std::errc::io_error : std::errc{};
}

bool recv(Packet &out) {
    std::lock_guard lock{g_mutex};
    if (g_queue.empty()) return false;
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

void reset() noexcept {
    std::lock_guard lock{g_mutex};
    g_queue.clear();
}

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

/**
 * @file net_driver.cpp
 * @brief UDP based network driver for Lattice IPC.
 */

#include "net_driver.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <deque>
#include <ifaddrs.h>
#include <mutex>
#include <net/if.h>
#include <netpacket/packet.h>
#include <thread>
#include <unordered_map>

namespace net {
namespace {
Config g_cfg{};                                    //!< active configuration
int g_socket{-1};                                  //!< UDP socket descriptor
std::unordered_map<node_t, sockaddr_in> g_remotes; //!< node mapping table
std::deque<Packet> g_queue;                        //!< received packets
std::mutex g_mutex;                                //!< protects g_queue
RecvCallback g_callback;                           //!< user callback
std::jthread g_thread;                             //!< background receiver
std::atomic<bool> g_running{false};

/**
 * @brief Hash a sequence of bytes into a node identifier.
 *
 * @param data Pointer to the byte sequence.
 * @param len  Number of bytes to hash.
 * @return Deterministic integer suitable for use as a node ID.
 */
[[nodiscard]] node_t hash_bytes(const std::byte *data, std::size_t len) {
    std::size_t value = 0;
    for (std::size_t i = 0; i < len; ++i) {
        value = value * 131 + std::to_integer<unsigned char>(data[i]);
    }
    return static_cast<node_t>(value & 0x7fffffff);
}

/**
 * @brief Derive a node identifier from the primary network interface.
 *
 * The function prefers a hardware address if available. When no non-loopback
 * interface is found, the host name is hashed as a fallback.
 */
[[nodiscard]] node_t derive_node_id() {
    ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
            if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) {
                continue;
            }
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_PACKET) {
                auto *ll = reinterpret_cast<sockaddr_ll *>(cur->ifa_addr);
                node_t id = hash_bytes(reinterpret_cast<std::byte *>(ll->sll_addr), ll->sll_halen);
                freeifaddrs(ifa);
                return id;
            }
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(cur->ifa_addr);
                node_t id = hash_bytes(reinterpret_cast<std::byte *>(&sin->sin_addr),
                                       sizeof(sin->sin_addr));
                freeifaddrs(ifa);
                return id;
            }
        }
        freeifaddrs(ifa);
    }
    char host[256]{};
    if (gethostname(host, sizeof(host)) == 0) {
        return static_cast<node_t>(std::hash<std::string_view>{}(std::string_view{host}));
    }
    return 0;
}

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
} // namespace

/**
 * @brief Initialize networking using the supplied configuration.
 *
 * When @p cfg specifies a node ID of zero, the identifier is derived from the
 * active network interface via ::derive_node_id. The function binds an IPv4
 * UDP socket on @p cfg.port and spawns a background thread to receive datagrams.
 *
 * @param cfg Driver configuration structure.
 */
void init(const Config &cfg) {
    g_cfg = cfg;
    if (g_cfg.node_id == 0) {
        g_cfg.node_id = derive_node_id();
    }
    g_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(g_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error{"bind failed"};
    }
    g_running.store(true, std::memory_order_relaxed);
    g_thread = std::jthread{recv_loop};
}

/**
 * @brief Record a remote node's address for outbound traffic.
 *
 * @param node Logical node identifier.
 * @param host Remote IPv4 address in dotted notation.
 * @param port UDP port on which the peer listens.
 */
void add_remote(node_t node, const std::string &host, std::uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_aton(host.c_str(), &addr.sin_addr);
    g_remotes[node] = addr;
}

/**
 * @brief Install a packet receive callback.
 *
 * The callback runs on the background thread whenever a datagram is dequeued.
 *
 * @param cb Callback functor to invoke.
 */
void set_recv_callback(RecvCallback cb) { g_callback = std::move(cb); }

/**
 * @brief Obtain the currently configured node identifier.
 */
node_t local_node() noexcept { return g_cfg.node_id; }

/**
 * @brief Transmit a payload to a remote node.
 *
 * The function prepends the local node ID and issues a UDP datagram to the
 * address registered via ::add_remote.
 *
 * @param node Destination node identifier.
 * @param data Payload bytes to send.
 */
void send(node_t node, std::span<const std::byte> data) {
    auto it = g_remotes.find(node);
    if (it == g_remotes.end()) {
        return; // unknown destination
    }
    std::vector<std::byte> buf(sizeof(node_t) + data.size());
    std::memcpy(buf.data(), &g_cfg.node_id, sizeof(node_t));
    std::memcpy(buf.data() + sizeof(node_t), data.data(), data.size());

    ::sendto(g_socket, buf.data(), buf.size(), 0, reinterpret_cast<const sockaddr *>(&it->second),
             sizeof(sockaddr_in));
}

/**
 * @brief Retrieve the next received packet for the local node.
 *
 * @param out Packet object populated with incoming data.
 * @return `true` when a packet was available, otherwise `false`.
 */
bool recv(Packet &out) {
    std::lock_guard<std::mutex> lock{g_mutex};
    if (g_queue.empty()) {
        return false;
    }
    out = std::move(g_queue.front());
    g_queue.pop_front();
    return true;
}

/**
 * @brief Remove all queued packets across every node.
 */
void reset() noexcept {
    std::lock_guard<std::mutex> lock{g_mutex};
    g_queue.clear();
}

/**
 * @brief Terminate networking and cleanup all resources.
 */
void shutdown() noexcept {
    g_running.store(false, std::memory_order_relaxed);
    if (g_socket != -1) {
        ::close(g_socket);
        g_socket = -1;
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
    {
        std::lock_guard<std::mutex> lock{g_mutex};
        g_queue.clear();
    }
    g_remotes.clear();
}

} // namespace net

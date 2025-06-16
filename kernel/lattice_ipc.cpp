/*
 * @file lattice_ipc.cpp
 * @brief Reference‐counted message buffer and lattice IPC logic.
 */

#include "lattice_ipc.hpp"

#include "../h/const.hpp"
#include "../h/error.hpp"

#include "../include/xinim/core_types.hpp"
#include "glo.hpp"
#include "net_driver.hpp"
#include "proc.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <span>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {

/**
 * @brief XOR-based symmetric cipher for message payloads.
 *
 * Applies a repeating XOR mask derived from @p key over @p buf in place.
 * Encryption and decryption are identical.
 *
 * @param buf  Span of bytes to transform.
 * @param key  Span of secret bytes used as mask.
 */
void xor_cipher(std::span<std::byte> buf, std::span<const std::byte> key) noexcept {
    for (std::size_t i = 0; i < buf.size(); ++i) {
        buf[i] ^= key[i % key.size()];
    }
}

} // anonymous namespace

namespace lattice {

/*==============================================================================
 *                           MessageBuffer Class
 *============================================================================*/

/**
 * @class MessageBuffer
 * @brief Shared, reference‐counted container for IPC message bytes.
 *
 * Uses std::shared_ptr<std::vector<std::byte>> internally so that multiple
 * parties can hold views without copying. Lifetime is managed automatically.
 */
class MessageBuffer {
  public:
    using Byte = std::byte;

    MessageBuffer();
    explicit MessageBuffer(std::size_t size);

    std::span<Byte> span() noexcept;
    std::span<const Byte> span() const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    std::shared_ptr<std::vector<Byte>> share() const noexcept;

  private:
    std::shared_ptr<std::vector<Byte>> data_;
};
/**
 * @brief Default construct an empty message buffer.
 */
MessageBuffer::MessageBuffer() = default;

/**
 * @brief Construct a buffer with @p size bytes initialized to zero.
 */
MessageBuffer::MessageBuffer(std::size_t size) : data_(std::make_shared<std::vector<Byte>>(size)) {}

/**
 * @brief Obtain a mutable view of the underlying bytes.
 */
std::span<MessageBuffer::Byte> MessageBuffer::span() noexcept {
    return data_ ? std::span<Byte>{data_->data(), data_->size()} : std::span<Byte>{};
}

/**
 * @brief Obtain a read-only view of the buffer contents.
 */
std::span<const MessageBuffer::Byte> MessageBuffer::span() const noexcept {
    return data_ ? std::span<const Byte>{data_->data(), data_->size()} : std::span<const Byte>{};
}

/**
 * @brief Total number of bytes stored in the buffer.
 */
std::size_t MessageBuffer::size() const noexcept { return data_ ? data_->size() : 0; }

/**
 * @brief Share ownership of the underlying storage.
 */
std::shared_ptr<std::vector<MessageBuffer::Byte>> MessageBuffer::share() const noexcept {
    return data_;
}

/*==============================================================================
 *                            Graph & Channel Logic
 *============================================================================*/

Graph g_graph; ///< Global IPC graph instance

/**
 * @brief Create or retrieve a channel between two processes.
 */
Channel &Graph::connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    auto key = std::make_tuple(src, dst, node_id);
    auto it = edges_.find(key);
    if (it != edges_.end()) {
        return it->second;
    }

    // Generate per‐endpoint keys and derive shared secret
    auto kp_src = pqcrypto::generate_keypair();
    auto kp_dst = pqcrypto::generate_keypair();

    Channel channel{};
    channel.src = src;
    channel.dst = dst;
    channel.node_id = node_id;
    channel.secret = pqcrypto::compute_shared_secret(kp_src, kp_dst);

    auto [ins_it, _] = edges_.emplace(key, std::move(channel));
    return ins_it->second;
}

/**
 * @brief Lookup a channel optionally searching all nodes.
 */
Channel *Graph::find(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) noexcept {
    if (node_id != ANY_NODE) {
        auto key = std::make_tuple(src, dst, node_id);
        auto it = edges_.find(key);
        return (it != edges_.end()) ? &it->second : nullptr;
    }
    for (auto &[key, ch] : edges_) {
        if (std::get<0>(key) == src && std::get<1>(key) == dst) {
            return &ch;
        }
    }
    return nullptr;
}

/**
 * @brief Test whether a process is waiting for a message.
 */
bool Graph::is_listening(xinim::pid_t pid) const noexcept {
    auto it = listening_.find(pid);
    return (it != listening_.end()) && it->second;
}

/**
 * @brief Set or clear the listening flag for a process.
 */
void Graph::set_listening(xinim::pid_t pid, bool flag) noexcept { listening_[pid] = flag; }

/*==============================================================================
 *                              IPC API Wrappers
 *============================================================================*/

int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    g_graph.connect(src, dst, node_id);
    return OK;
}

void lattice_listen(xinim::pid_t pid) { g_graph.set_listening(pid, true); }

/**
 * @brief Yield execution context to another process.
 */
/**
 * @brief Switch execution to the specified process.
 */
static void yield_to(pid_t pid) {
    proc_ptr = proc_addr(pid);
    cur_proc = pid;
}

/**
 * @brief Send a message, creating a channel if necessary.
 */
int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg) {
    // Ensure channel exists (local or remote)
    Channel *ch = g_graph.find(src, dst, ANY_NODE);
    if (!ch) {
        ch = &g_graph.connect(src, dst, net::local_node());
    }

    // Remote‐node delivery over network
    if (ch->node_id != net::local_node()) {
        std::vector<std::byte> packet(sizeof(xinim::pid_t) * 2 + sizeof(message));
        auto *ids = reinterpret_cast<xinim::pid_t *>(packet.data());
        ids[0] = src;
        ids[1] = dst;
        std::memcpy(packet.data() + sizeof(xinim::pid_t) * 2, &msg, sizeof(message));
        std::span<const std::byte> bytes(packet.data(), packet.size());
        net::send(ch->node_id, bytes);
        return OK;
    }

    // Local delivery path
    if (g_graph.is_listening(dst)) {
        // Direct handoff
        g_graph.inbox_[dst] = msg;
        g_graph.set_listening(dst, false);
        yield_to(dst);
    } else {
        // Encrypt in place and queue
        message copy = msg;
        auto buf = std::span<std::byte>(reinterpret_cast<std::byte *>(&copy), sizeof(message));
        xor_cipher(
            buf, std::span<const std::byte>(reinterpret_cast<const std::byte *>(ch->secret.data()),
                                            ch->secret.size()));
        ch->queue.push_back(std::move(copy));
    }

    return OK;
}

/**
 * @brief Receive a pending message for @p pid.
 */
int lattice_recv(xinim::pid_t pid, message *out) {
    // 1) Check inbox (direct handoff)
    auto ib = g_graph.inbox_.find(pid);
    if (ib != g_graph.inbox_.end()) {
        *out = ib->second;
        g_graph.inbox_.erase(ib);
        return OK;
    }

    // 2) Scan queued channels
    for (auto &[key, ch] : g_graph.edges_) {
        if (std::get<1>(key) != pid || std::get<2>(key) != net::local_node() || ch.queue.empty()) {
            continue;
        }

        // Dequeue, decrypt in place, and return
        *out = std::move(ch.queue.front());
        ch.queue.erase(ch.queue.begin());

        auto buf = std::span<std::byte>(reinterpret_cast<std::byte *>(out), sizeof(message));
        xor_cipher(buf,
                   std::span<const std::byte>(reinterpret_cast<const std::byte *>(ch.secret.data()),
                                              ch.secret.size()));
        return OK;
    }

    // 3) No message: register as listener
    lattice_listen(pid);
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

void poll_network() {
    net::Packet pkt{};
    while (net::recv(pkt)) {
        if (pkt.payload.size() != sizeof(xinim::pid_t) * 2 + sizeof(message)) {
            continue;
        }
        auto *ids = reinterpret_cast<const xinim::pid_t *>(pkt.payload.data());
        xinim::pid_t src = ids[0];
        xinim::pid_t dst = ids[1];
        message msg{};
        std::memcpy(reinterpret_cast<void *>(&msg), pkt.payload.data() + sizeof(xinim::pid_t) * 2,
                    sizeof(message));

        Channel *ch = g_graph.find(src, dst, pkt.src_node);
        if (!ch) {
            ch = &g_graph.connect(src, dst, pkt.src_node);
        }

        auto buf = std::span<std::byte>(reinterpret_cast<std::byte *>(&msg), sizeof(message));
        xor_cipher(
            buf, std::span<const std::byte>(reinterpret_cast<const std::byte *>(ch->secret.data()),
                                            ch->secret.size()));
        ch->queue.push_back(std::move(msg));
    }
}

} // namespace lattice

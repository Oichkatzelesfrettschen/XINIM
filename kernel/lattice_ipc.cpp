/*
 * @file lattice_ipc.cpp
 * @brief Reference‐counted message buffer and lattice IPC logic.
 */

#include "lattice_ipc.hpp"

#include "../h/const.hpp"
#include "../h/error.hpp"

#include "glo.hpp"
#include "net_driver.hpp"
#include "proc.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>
#include <tuple>
#include <deque>
#include <unordered_map>

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
void xor_cipher(std::span<std::byte> buf,
                std::span<const std::byte> key) noexcept
{
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
MessageBuffer::MessageBuffer() = default;

MessageBuffer::MessageBuffer(std::size_t size)
    : data_(std::make_shared<std::vector<Byte>>(size))
{}

std::span<MessageBuffer::Byte> MessageBuffer::span() noexcept {
    return data_
        ? std::span<Byte>{ data_->data(), data_->size() }
        : std::span<Byte>{};
}

std::span<const MessageBuffer::Byte> MessageBuffer::span() const noexcept {
    return data_
        ? std::span<const Byte>{ data_->data(), data_->size() }
        : std::span<const Byte>{};
}

std::size_t MessageBuffer::size() const noexcept {
    return data_ ? data_->size() : 0;
}

std::shared_ptr<std::vector<MessageBuffer::Byte>>
MessageBuffer::share() const noexcept {
    return data_;
}

/*==============================================================================
 *                            Graph & Channel Logic
 *============================================================================*/

Graph g_graph;  ///< Global IPC graph instance

Channel &Graph::connect(pid_t src,
                        pid_t dst,
                        net::node_t node_id)
{
    auto key = std::make_tuple(src, dst, node_id);
    auto it  = edges_.find(key);
    if (it != edges_.end()) {
        return it->second;
    }

    // Generate per‐endpoint keys and derive shared secret
    auto kp_src = pqcrypto::generate_keypair();
    auto kp_dst = pqcrypto::generate_keypair();

    Channel channel{};
    channel.src     = src;
    channel.dst     = dst;
    channel.node_id = node_id;
    channel.secret  = pqcrypto::compute_shared_secret(
                          kp_src.private_key,
                          kp_dst.public_key);

    auto [ins_it, _] = edges_.emplace(key, std::move(channel));
    return ins_it->second;
}

Channel *Graph::find(pid_t src,
                     pid_t dst,
                     net::node_t node_id) noexcept
{
    auto key = std::make_tuple(src, dst, node_id);
    auto it  = edges_.find(key);
    return (it != edges_.end()) ? &it->second : nullptr;
}

Channel *Graph::find_any(pid_t src, pid_t dst) noexcept {
    for (auto & [key, ch] : edges_) {
        if (std::get<0>(key) == src && std::get<1>(key) == dst) {
            return &ch;
        }
    }
    return nullptr;
}

bool Graph::is_listening(pid_t pid) const noexcept {
    auto it = listening_.find(pid);
    return (it != listening_.end()) && it->second;
}

void Graph::set_listening(pid_t pid, bool flag) noexcept {
    listening_[pid] = flag;
}

/*==============================================================================
 *                              IPC API Wrappers
 *============================================================================*/

int lattice_connect(pid_t src,
                    pid_t dst,
                    net::node_t node_id)
{
    g_graph.connect(src, dst, node_id);
    return static_cast<int>(ErrorCode::OK);
}

void lattice_listen(pid_t pid) {
    g_graph.set_listening(pid, true);
}

/**
 * @brief Yield execution context to another process.
 */
static void yield_to(pid_t pid) {
    proc_ptr = proc_addr(pid);
    cur_proc = pid;
}

int lattice_send(pid_t src,
                 pid_t dst,
                 const message &msg)
{
    // Ensure channel exists (local or remote)
    Channel *ch = g_graph.find_any(src, dst);
    if (!ch) {
        ch = &g_graph.connect(src, dst, net::local_node());
    }

    // Remote‐node delivery over network
    if (ch->node_id != net::local_node()) {
        std::span<const std::byte> bytes{
            reinterpret_cast<const std::byte*>(&msg),
            sizeof(message)
        };
        net::send(ch->node_id, bytes);
        return static_cast<int>(ErrorCode::OK);
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
        auto buf = std::span<std::byte>(
            reinterpret_cast<std::byte*>(&copy),
            sizeof(message)
        );
        xor_cipher(buf, std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(ch->secret.data()),
            ch->secret.size()
        ));
        ch->queue.push_back(std::move(copy));
    }

    return static_cast<int>(ErrorCode::OK);
}

int lattice_recv(pid_t pid, message *out) {
    // 1) Check inbox (direct handoff)
    auto ib = g_graph.inbox_.find(pid);
    if (ib != g_graph.inbox_.end()) {
        *out = ib->second;
        g_graph.inbox_.erase(ib);
        return static_cast<int>(ErrorCode::OK);
    }

    // 2) Scan queued channels
    for (auto & [key, ch] : g_graph.edges_) {
        if (std::get<1>(key) != pid ||
            std::get<2>(key) != net::local_node() ||
            ch.queue.empty())
        {
            continue;
        }

        // Dequeue, decrypt in place, and return
        *out = std::move(ch.queue.front());
        ch.queue.pop_front();

        auto buf = std::span<std::byte>(
            reinterpret_cast<std::byte*>(out),
            sizeof(message)
        );
        xor_cipher(buf, std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(ch.secret.data()),
            ch.secret.size()
        ));
        return static_cast<int>(ErrorCode::OK);
    }

    // 3) No message: register as listener
    lattice_listen(pid);
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

} // namespace lattice

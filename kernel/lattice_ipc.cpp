/*
 * @file lattice_ipc.cpp
 * @brief Capability‐based, post‐quantum IPC with optional non-blocking support.
 */

#include "lattice_ipc.hpp"

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../include/xinim/core_types.hpp"
#include "glo.hpp"
#include "net_driver.hpp"
#include "octonion.hpp"
#include "octonion_math.hpp"
#include "schedule.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <deque>
#include <span>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {

/**
 * @brief XOR‐stream cipher using an Octonion as key.
 *
 * Encryption and decryption are identical.
 */
void xor_cipher(std::span<std::byte> buf, const OctonionToken &key) noexcept {
    std::array<uint8_t, Octonion::ByteCount> mask{};
    key.value.to_bytes(mask);
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] ^= std::byte{mask[i % mask.size()]};
    }
}

} // anonymous namespace

namespace lattice {

/*==============================================================================
 *                               Global State
 *============================================================================*/

Graph g_graph; ///< Singleton IPC graph

/*==============================================================================
 *                            Graph Implementation
 *============================================================================*/

Channel &Graph::connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    auto key = std::make_tuple(src, dst, node_id);
    auto it = edges_.find(key);
    if (it != edges_.end()) {
        return it->second;
    }
    Channel ch{};
    ch.src = src;
    ch.dst = dst;
    ch.node_id = node_id;
    edges_.emplace(key, std::move(ch));
    return edges_[key];
}

Channel *Graph::find(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) noexcept {
    if (node_id != ANY_NODE) {
        auto key = std::make_tuple(src, dst, node_id);
        auto it = edges_.find(key);
        if (it != edges_.end()) {
            return &it->second;
        }
    } else {
        for (auto &[k, ch] : edges_) {
            if (std::get<0>(k) == src && std::get<1>(k) == dst) {
                return &ch;
            }
        }
    }
    return nullptr;
}

bool Graph::is_listening(xinim::pid_t pid) const noexcept {
    auto it = listening_.find(pid);
    return it != listening_.end() && it->second;
}

void Graph::set_listening(xinim::pid_t pid, bool flag) noexcept { listening_[pid] = flag; }

/*==============================================================================
 *                               IPC API
 *============================================================================*/

/**
 * @brief Establish bidirectional channels and perform stubbed Kyber exchange.
 */
int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    auto kp_a = pqcrypto::generate_keypair();
    auto kp_b = pqcrypto::generate_keypair();
    auto secret_bytes = pqcrypto::compute_shared_secret(kp_a, kp_b);
    OctonionToken secret{Octonion::from_bytes(secret_bytes)};

    auto &fwd = g_graph.connect(src, dst, node_id);
    auto &bwd = g_graph.connect(dst, src, node_id);
    fwd.secret = secret;
    bwd.secret = secret;
    return OK;
}

/**
 * @brief Mark @p pid as listening for a direct handoff.
 */
void lattice_listen(xinim::pid_t pid) { g_graph.set_listening(pid, true); }

/**
 * @brief Send a message, with optional non-blocking behavior.
 *
 * @param src    Sender PID.
 * @param dst    Receiver PID.
 * @param msg    Message to send.
 * @param flags  IpcFlags::NONBLOCK to avoid blocking on queue.
 * @return OK, E_TRY_AGAIN, or E_NO_MESSAGE.
 */
int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, IpcFlags flags) {
    Channel *ch = g_graph.find(src, dst, ANY_NODE);
    if (!ch) {
        ch = &g_graph.connect(src, dst, net::local_node());
    }

    // Remote‐node delivery
    if (ch->node_id != net::local_node()) {
        std::vector<std::byte> pkt(sizeof(xinim::pid_t) * 2 + sizeof(msg));
        auto *ids = reinterpret_cast<xinim::pid_t *>(pkt.data());
        ids[0] = src;
        ids[1] = dst;
        std::memcpy(pkt.data() + sizeof(xinim::pid_t) * 2, &msg, sizeof(msg));
        xor_cipher({pkt.data(), pkt.size()}, ch->secret);
        if (!net::send(ch->node_id, pkt)) {
            return static_cast<int>(ErrorCode::EIO);
        }
        return OK;
    }

    // Local direct handoff
    if (g_graph.is_listening(dst)) {
        g_graph.inbox_[dst] = msg;
        g_graph.set_listening(dst, false);
        schedule::scheduler.yield_to(dst);
        return OK;
    }

    // Non-blocking: do not queue
    if (flags == IpcFlags::NONBLOCK) {
        return static_cast<int>(ErrorCode::E_TRY_AGAIN);
    }

    // Blocking: encrypt in-place and enqueue
    message copy = msg;
    xor_cipher({reinterpret_cast<std::byte *>(&copy), sizeof(copy)}, ch->secret);
    ch->queue.push_back(std::move(copy));
    return OK;
}

/**
 * @brief Receive a pending message, with optional non-blocking behavior.
 *
 * @param pid    Receiver PID.
 * @param out    Pointer to message struct for output.
 * @param flags  IpcFlags::NONBLOCK to avoid blocking.
 * @return OK or E_NO_MESSAGE.
 */
int lattice_recv(xinim::pid_t pid, message *out, IpcFlags flags) {
    // 1) Direct-handoff inbox
    auto ib = g_graph.inbox_.find(pid);
    if (ib != g_graph.inbox_.end()) {
        *out = ib->second;
        g_graph.inbox_.erase(ib);
        return OK;
    }

    // 2) Dequeue from any matching channel
    for (auto &[key, ch] : g_graph.edges_) {
        if (std::get<1>(key) == pid && std::get<2>(key) == net::local_node() && !ch.queue.empty()) {
            message copy = std::move(ch.queue.front());
            ch.queue.pop_front();
            xor_cipher({reinterpret_cast<std::byte *>(&copy), sizeof(copy)}, ch->secret);
            *out = std::move(copy);
            return OK;
        }
    }

    // 3) Non-blocking: return immediately
    if (flags == IpcFlags::NONBLOCK) {
        return static_cast<int>(ErrorCode::E_NO_MESSAGE);
    }

    // 4) Blocking: register listener and block
    lattice_listen(pid);
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

/**
 * @brief Poll the network for incoming packets and enqueue them.
 *
 * Decrypts payloads and places them into the appropriate channel queue.
 */
void poll_network() {
    net::Packet pkt;
    while (net::recv(pkt)) {
        const auto &p = pkt.payload;
        if (p.size() != sizeof(xinim::pid_t) * 2 + sizeof(message)) {
            continue;
        }
        auto ids = reinterpret_cast<const xinim::pid_t *>(p.data());
        xinim::pid_t src = ids[0], dst = ids[1];
        message msg;
        std::memcpy(&msg, p.data() + sizeof(xinim::pid_t) * 2, sizeof(msg));

        Channel *ch = g_graph.find(src, dst, pkt.src_node);
        if (!ch) {
            ch = &g_graph.connect(src, dst, pkt.src_node);
        }
        xor_cipher({reinterpret_cast<std::byte *>(&msg), sizeof(msg)}, ch->secret);
        ch->queue.push_back(std::move(msg));
    }
}

} // namespace lattice

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
#include "octonion.hpp"
#include "proc.hpp"

#include <algorithm>
#include <array>
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
 * @brief XOR-stream cipher for payloads using an Octonion as key.
 *
 * Encryption and decryption are identical.
 */
void xor_cipher(std::span<std::byte> buf, const Octonion &key) noexcept {
    std::array<uint8_t, Octonion::ByteCount> mask{};
    key.to_bytes(mask);
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] ^= std::byte{mask[i % mask.size()]};
    }
}

} // namespace

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

static void yield_to(xinim::pid_t pid) {
    proc_ptr = proc_addr(pid);
    cur_proc = pid;
}

int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    // Perform stubbed Kyber key exchange
    auto kp_a = pqcrypto::generate_keypair();
    auto kp_b = pqcrypto::generate_keypair();
    auto secret_bytes = pqcrypto::compute_shared_secret(kp_a, kp_b);
    Octonion secret = Octonion::from_bytes(secret_bytes);

    // Create forward and reverse channels
    Channel &fwd = g_graph.connect(src, dst, node_id);
    Channel &bwd = g_graph.connect(dst, src, node_id);
    fwd.secret = secret;
    bwd.secret = secret;
    return OK;
}

void lattice_listen(xinim::pid_t pid) { g_graph.set_listening(pid, true); }

int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg) {
    Channel *ch = g_graph.find(src, dst, ANY_NODE);
    if (!ch) {
        ch = &g_graph.connect(src, dst, net::local_node());
    }

    // Remote delivery handled by the UDP driver
    if (ch->node_id != net::local_node()) {
        std::vector<std::byte> pkt(sizeof(xinim::pid_t) * 2 + sizeof(msg));
        auto *ids = reinterpret_cast<xinim::pid_t *>(pkt.data());
        ids[0] = src;
        ids[1] = dst;
        std::memcpy(pkt.data() + sizeof(xinim::pid_t) * 2, &msg, sizeof(msg));
        xor_cipher({pkt.data(), pkt.size()}, ch->secret);
        net::send(ch->node_id, pkt);
        return OK;
    }

    // Local delivery: direct handoff or queue
    if (g_graph.is_listening(dst)) {
        g_graph.inbox_[dst] = msg;
        g_graph.set_listening(dst, false);
        yield_to(dst);
    } else {
        message copy = msg;
        xor_cipher({reinterpret_cast<std::byte *>(&copy), sizeof(copy)}, ch->secret);
        ch->queue.push_back(std::move(copy));
    }
    return OK;
}

int lattice_recv(xinim::pid_t pid, message *out) {
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

    // 3) No message: register listener
    lattice_listen(pid);
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

//------------------------------------------------------------------------------
/// Drain the UDP driver queue and populate channel inboxes.
void poll_network() {
    net::Packet pkt;
    while (net::recv(pkt)) {
        auto &payload = pkt.payload;
        if (payload.size() != sizeof(xinim::pid_t) * 2 + sizeof(message)) {
            continue;
        }
        auto ids = reinterpret_cast<const xinim::pid_t *>(payload.data());
        xinim::pid_t src = ids[0], dst = ids[1];
        message msg;
        std::memcpy(&msg, payload.data() + sizeof(xinim::pid_t) * 2, sizeof(msg));

        Channel *ch = g_graph.find(src, dst, pkt.src_node);
        if (!ch) {
            ch = &g_graph.connect(src, dst, pkt.src_node);
        }
        xor_cipher({reinterpret_cast<std::byte *>(&msg), sizeof(msg)}, ch->secret);
        ch->queue.push_back(std::move(msg));
    }
}

} // namespace lattice

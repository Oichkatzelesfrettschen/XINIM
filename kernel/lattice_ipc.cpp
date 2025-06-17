/*
 * @file lattice_ipc.cpp
 * @brief Capability‐based, post‐quantum IPC with optional non-blocking support.
 */

#include "lattice_ipc.hpp"

#include "../h/const.hpp"
#include "../include/xinim/core_types.hpp"
#include "glo.hpp"
#include "net_driver.hpp"
#include "octonion.hpp"
#include "octonion_math.hpp"
#include "schedule.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <sodium.h>
#include <span>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../h/error.hpp"

namespace {

/** Length of the nonce used with ChaCha20-Poly1305. */
constexpr std::size_t NONCE_SIZE = crypto_aead_chacha20poly1305_ietf_NPUBBYTES;

/** Length of the authentication tag appended to each message. */
constexpr std::size_t TAG_SIZE = AEAD_TAG_SIZE;

/// Convert a sequence number to a 12-byte nonce.
[[nodiscard]] static std::array<std::byte, NONCE_SIZE> make_nonce(std::uint64_t seq) noexcept {
    std::array<std::byte, NONCE_SIZE> n{};
    std::memcpy(n.data(), &seq, std::min(sizeof(seq), static_cast<std::size_t>(NONCE_SIZE)));
    return n;
}

/// Derive a ChaCha20 key from an OctonionToken.
[[nodiscard]] static std::array<std::byte, crypto_aead_chacha20poly1305_ietf_KEYBYTES>
token_to_key(const lattice::OctonionToken &token) noexcept {
    std::array<std::uint8_t, 32> raw{};
    token.value.to_bytes(raw);
    std::array<std::byte, crypto_aead_chacha20poly1305_ietf_KEYBYTES> k{};
    std::memcpy(k.data(), raw.data(), k.size());
    return k;
}

/// Encrypt a buffer in place producing a detached authentication tag.
static void aead_encrypt(std::span<std::byte> buf, const lattice::OctonionToken &key_token,
                         std::uint64_t seq, std::array<std::byte, TAG_SIZE> &tag) {
    std::array<std::byte, NONCE_SIZE> nonce = make_nonce(seq);
    auto key = token_to_key(key_token);
    std::array<std::byte, sizeof(message) + TAG_SIZE> tmp{};
    unsigned long long out_len = 0;
    crypto_aead_chacha20poly1305_ietf_encrypt(
        reinterpret_cast<unsigned char *>(tmp.data()), &out_len,
        reinterpret_cast<const unsigned char *>(buf.data()), buf.size(), nullptr, 0, nullptr,
        reinterpret_cast<const unsigned char *>(nonce.data()),
        reinterpret_cast<const unsigned char *>(key.data()));
    std::copy_n(tmp.begin(), buf.size(), buf.begin());
    std::copy_n(tmp.begin() + buf.size(), TAG_SIZE, tag.begin());
}

/// Decrypt a buffer in place verifying the authentication tag.
[[nodiscard]] static bool aead_decrypt(std::span<std::byte> buf,
                                       const lattice::OctonionToken &key_token, std::uint64_t seq,
                                       std::span<const std::byte, TAG_SIZE> tag) {
    std::array<std::byte, NONCE_SIZE> nonce = make_nonce(seq);
    auto key = token_to_key(key_token);
    std::array<std::byte, sizeof(message) + TAG_SIZE> tmp{};
    std::copy_n(buf.begin(), buf.size(), tmp.begin());
    std::copy_n(tag.begin(), TAG_SIZE, tmp.begin() + buf.size());
    unsigned long long out_len = 0;
    if (crypto_aead_chacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char *>(buf.data()), &out_len, nullptr,
            reinterpret_cast<const unsigned char *>(tmp.data()), tmp.size(), nullptr, 0,
            reinterpret_cast<const unsigned char *>(nonce.data()),
            reinterpret_cast<const unsigned char *>(key.data())) != 0) {
        return false;
    }
    return true;
}

} // anonymous namespace

namespace lattice {

/*==============================================================================
 *                               Global State
 *============================================================================*/

Graph g_graph; ///< Singleton IPC graph

/** Mutex guarding IPC wait state. */
static std::mutex g_ipc_mutex;
/** Condition variable to wake waiting receivers. */
static std::condition_variable g_ipc_cv;

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
        std::vector<std::byte> pkt(sizeof(xinim::pid_t) * 2 + sizeof(msg) + TAG_SIZE);
        auto *ids = reinterpret_cast<xinim::pid_t *>(pkt.data());
        ids[0] = src;
        ids[1] = dst;
        std::memcpy(pkt.data() + sizeof(xinim::pid_t) * 2, &msg, sizeof(msg));
        std::span<std::byte> cipher{pkt.data() + sizeof(xinim::pid_t) * 2, sizeof(msg)};
        std::array<std::byte, TAG_SIZE> tag{};
        aead_encrypt(cipher, ch->secret, ch->tx_seq++, tag);
        std::memcpy(pkt.data() + sizeof(xinim::pid_t) * 2 + sizeof(msg), tag.data(), TAG_SIZE);
        if (net::send(ch->node_id, pkt) != std::errc{}) {
            return static_cast<int>(ErrorCode::EIO);
        }
        return OK;
    }

    // Local direct handoff
    if (g_graph.is_listening(dst)) {
        std::lock_guard lk(g_ipc_mutex);
        g_graph.inbox_[dst] = msg;
        g_graph.set_listening(dst, false);
        sched::scheduler.unblock(dst);
        g_ipc_cv.notify_all();
        sched::scheduler.yield_to(dst);
        return OK;
    }

    // Non-blocking: do not queue
    if (flags == IpcFlags::NONBLOCK) {
        return static_cast<int>(ErrorCode::E_TRY_AGAIN);
    }

    // Blocking: encrypt in-place and enqueue
    EncryptedMessage enc{};
    std::memcpy(enc.data.data(), &msg, sizeof(msg));
    aead_encrypt({enc.data.data(), sizeof(msg)}, ch->secret, ch->tx_seq++, enc.tag);
    ch->queue.push_back(std::move(enc));
    if (g_graph.is_listening(dst)) {
        g_graph.set_listening(dst, false);
        sched::scheduler.unblock(dst);
        g_ipc_cv.notify_all();
    }
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
            EncryptedMessage enc = std::move(ch.queue.front());
            ch.queue.pop_front();
            std::span<std::byte> buf{enc.data.data(), sizeof(message)};
            if (!aead_decrypt(buf, ch.secret, ch.rx_seq++, enc.tag)) {
                return static_cast<int>(ErrorCode::E_NO_MESSAGE);
            }
            std::memcpy(out, enc.data.data(), sizeof(message));
            return OK;
        }
    }

    // 3) Non-blocking: return immediately
    if (flags == IpcFlags::NONBLOCK) {
        return static_cast<int>(ErrorCode::E_NO_MESSAGE);
    }

    // 4) Blocking: register listener and wait with timeout
    using namespace std::chrono_literals;
    std::unique_lock lk(g_ipc_mutex);
    lattice_listen(pid);
    sched::scheduler.block_on(pid, -1);
    auto deadline = std::chrono::steady_clock::now() + 100ms;
    for (;;) {
        auto ib2 = g_graph.inbox_.find(pid);
        if (ib2 != g_graph.inbox_.end()) {
            *out = ib2->second;
            g_graph.inbox_.erase(ib2);
            sched::scheduler.unblock(pid);
            g_graph.set_listening(pid, false);
            return OK;
        }
        for (auto &[key, ch] : g_graph.edges_) {
            if (std::get<1>(key) == pid && std::get<2>(key) == net::local_node() &&
                !ch.queue.empty()) {
                EncryptedMessage enc = std::move(ch.queue.front());
                ch.queue.pop_front();
                std::span<std::byte> buf{enc.data.data(), sizeof(message)};
                if (!aead_decrypt(buf, ch.secret, ch.rx_seq++, enc.tag)) {
                    continue;
                }
                std::memcpy(out, enc.data.data(), sizeof(message));
                sched::scheduler.unblock(pid);
                g_graph.set_listening(pid, false);
                return OK;
            }
        }
        if (g_ipc_cv.wait_until(lk, deadline) == std::cv_status::timeout) {
            sched::scheduler.unblock(pid);
            g_graph.set_listening(pid, false);
            return static_cast<int>(ErrorCode::E_NO_MESSAGE);
        }
    }
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
        if (p.size() != sizeof(xinim::pid_t) * 2 + sizeof(message) + TAG_SIZE) {
            continue;
        }
        auto ids = reinterpret_cast<const xinim::pid_t *>(p.data());
        xinim::pid_t src = ids[0], dst = ids[1];
        EncryptedMessage enc{};
        std::memcpy(enc.data.data(), p.data() + sizeof(xinim::pid_t) * 2, sizeof(message));
        std::memcpy(enc.tag.data(), p.data() + sizeof(xinim::pid_t) * 2 + sizeof(message),
                    TAG_SIZE);

        Channel *ch = g_graph.find(src, dst, pkt.src_node);
        if (!ch) {
            ch = &g_graph.connect(src, dst, pkt.src_node);
        }
        ch->queue.push_back(std::move(enc));
        if (g_graph.is_listening(dst)) {
            g_graph.set_listening(dst, false);
            sched::scheduler.unblock(dst);
            g_ipc_cv.notify_all();
        }
    }
}

} // namespace lattice

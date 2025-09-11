/*
 * @file lattice_ipc.cpp
 * @brief Capability‐based, post‐quantum IPC with optional non-blocking support.
 */

#include "lattice_ipc.hpp"

#include "sys/const.hpp"
#include "../include/xinim/core_types.hpp"
#include "glo.hpp"
#include "net_driver.hpp"
#include "schedule.hpp"
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <span>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "sys/error.hpp"

namespace {

/** Size of the nonce used with XChaCha20-Poly1305. */
constexpr std::size_t NONCE_SIZE = lattice::AEAD_NONCE_SIZE;

/** Size of the authentication tag appended to ciphertext. */
constexpr std::size_t TAG_SIZE = lattice::AEAD_TAG_SIZE;

/**
 * @brief Encrypt a buffer using the channel key.
 *
 * @param plain  Plaintext input span.
 * @param key    AEAD key.
 * @param nonce  Output buffer receiving the generated nonce.
 * @return Ciphertext with appended authentication tag.
 */
[[nodiscard]] std::vector<std::byte>
aead_encrypt(std::span<const std::byte> plain,
             std::span<const std::uint8_t, lattice::AEAD_KEY_SIZE> key,
             std::array<std::byte, NONCE_SIZE> &nonce) {
    if (sodium_init() < 0) {
        throw std::runtime_error{"sodium_init failed"};
    }
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<std::byte> cipher(plain.size() + TAG_SIZE);
    unsigned long long out_len = 0U;
    crypto_aead_xchacha20poly1305_ietf_encrypt(
        reinterpret_cast<unsigned char *>(cipher.data()), &out_len,
        reinterpret_cast<const unsigned char *>(plain.data()), plain.size(), nullptr, 0, nullptr,
        reinterpret_cast<const unsigned char *>(nonce.data()), key.data());
    cipher.resize(static_cast<std::size_t>(out_len));
    return cipher;
}

/**
 * @brief Decrypt a buffer using the channel key.
 *
 * @param cipher Ciphertext with authentication tag.
 * @param key    AEAD key.
 * @param nonce  Nonce that accompanied the ciphertext.
 * @return Decrypted plaintext on success or empty vector on failure.
 */
[[nodiscard]] std::vector<std::byte>
aead_decrypt(std::span<const std::byte> cipher,
             std::span<const std::uint8_t, lattice::AEAD_KEY_SIZE> key,
             std::span<const std::byte, NONCE_SIZE> nonce) {
    if (sodium_init() < 0) {
        throw std::runtime_error{"sodium_init failed"};
    }
    std::vector<std::byte> plain(cipher.size() - TAG_SIZE);
    unsigned long long out_len = 0U;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char *>(plain.data()), &out_len, nullptr,
            reinterpret_cast<const unsigned char *>(cipher.data()), cipher.size(), nullptr, 0,
            reinterpret_cast<const unsigned char *>(nonce.data()), key.data()) != 0) {
        return {};
    }
    plain.resize(static_cast<std::size_t>(out_len));
    return plain;
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
    if (node_id == 0) {
        node_id = net::local_node();
    }

    auto kp_a = pqcrypto::generate_keypair();
    auto kp_b = pqcrypto::generate_keypair();
    auto secret_bytes = pqcrypto::compute_shared_secret(kp_a, kp_b);

    auto &fwd = g_graph.connect(src, dst, node_id);
    auto &bwd = g_graph.connect(dst, src, node_id);
    std::copy(secret_bytes.begin(), secret_bytes.end(), fwd.key.begin());
    std::copy(secret_bytes.begin(), secret_bytes.end(), bwd.key.begin());
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
        std::array<std::byte, NONCE_SIZE> nonce{};
        std::vector<std::byte> plain(sizeof(msg));
        std::memcpy(plain.data(), &msg, sizeof(msg));
        auto cipher = aead_encrypt(plain, ch->key, nonce);

        std::vector<std::byte> pkt(sizeof(xinim::pid_t) * 2 + nonce.size() + cipher.size());
        auto *ids = reinterpret_cast<xinim::pid_t *>(pkt.data());
        ids[0] = src;
        ids[1] = dst;
        std::memcpy(pkt.data() + sizeof(xinim::pid_t) * 2, nonce.data(), nonce.size());
        std::memcpy(pkt.data() + sizeof(xinim::pid_t) * 2 + nonce.size(), cipher.data(),
                    cipher.size());

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

    // Blocking: enqueue plaintext message
    ch->queue.push_back(msg);
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
            *out = ch.queue.front();
            ch.queue.pop_front();
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
    [[maybe_unused]] bool blocked = sched::scheduler.block_on(pid, -1);
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
                *out = ch.queue.front();
                ch.queue.pop_front();
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
        if (p.size() < sizeof(xinim::pid_t) * 2 + NONCE_SIZE + TAG_SIZE) {
            continue;
        }
        auto ids = reinterpret_cast<const xinim::pid_t *>(p.data());
        xinim::pid_t src = ids[0], dst = ids[1];
        std::array<std::byte, NONCE_SIZE> nonce{};
        std::memcpy(nonce.data(), p.data() + sizeof(xinim::pid_t) * 2, nonce.size());
        std::span<const std::byte> cipher{p.data() + sizeof(xinim::pid_t) * 2 + nonce.size(),
                                          p.size() - sizeof(xinim::pid_t) * 2 - nonce.size()};

        Channel *ch = g_graph.find(src, dst, pkt.src_node);
        if (!ch) {
            ch = &g_graph.connect(src, dst, pkt.src_node);
        }
        auto plain = aead_decrypt(cipher, ch->key, nonce);
        if (plain.size() != sizeof(message)) {
            continue;
        }
        message msg;
        std::memcpy(&msg, plain.data(), sizeof(msg));
        ch->queue.push_back(std::move(msg));
        if (g_graph.is_listening(dst)) {
            g_graph.set_listening(dst, false);
            sched::scheduler.unblock(dst);
            g_ipc_cv.notify_all();
        }
    }
}

} // namespace lattice

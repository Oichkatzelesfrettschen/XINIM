/**
 * @file test_lattice_ipc.cpp
 * @brief Unit test for lattice IPC primitives with Kyber-based PQ encryption.
 *
 * This test validates:
 *   1. Channel creation and queued delivery semantics.
 *   2. Immediate handoff when the receiver is listening.
 *   3. Proper indication of “no message” when the queue is drained.
 *
 * Verified June 15, 2025.
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "../crypto/kyber.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/const.hpp"
#include "../kernel/lattice_ipc.hpp"

using namespace lattice;

/**
 * @brief Convert a string to a vector of bytes.
 */
static std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> out;
    out.reserve(text.size());
    for (char c : text) {
        out.push_back(std::byte{static_cast<unsigned char>(c)});
    }
    return out;
}

/**
 * @brief Compare two byte spans for equality.
 */
static bool bytes_equal(std::span<const std::byte> a, std::span<const std::byte> b) noexcept {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

int main() {
    // Reset global IPC state
    g_graph = Graph{};

    constexpr pid_t SRC = 40;
    constexpr pid_t DST = 41;
    constexpr std::string_view PAYLOAD = "lattice secret";

    // ——— Phase 1: Channel creation & queued delivery ———
    assert(lattice_connect(SRC, DST) == OK);
    Channel *ch = g_graph.find(SRC, DST);
    assert(ch != nullptr && "Channel must exist after connect()");

    auto plaintext = to_bytes(PAYLOAD);
    auto kp = pq::kyber::keypair();
    auto cipher = pq::kyber::encrypt(plaintext, kp.public_key);

    message send_msg{};
    send_msg.m_type = 1;
    send_msg.m1_i1() = static_cast<int>(cipher.size());
    send_msg.m1_p1() = reinterpret_cast<char *>(cipher.data());

    // Enqueue and verify
    assert(lattice_send(SRC, DST, send_msg) == OK);
    assert(!ch->queue.empty() && "Message should be queued");

    // Dequeue, decrypt, and compare
    message recv_msg{};
    assert(lattice_recv(DST, &recv_msg) == OK);
    std::span<const std::byte> recv_bytes{reinterpret_cast<const std::byte *>(recv_msg.m1_p1()),
                                          static_cast<std::size_t>(recv_msg.m1_i1())};
    auto decrypted1 = pq::kyber::decrypt(recv_bytes, kp.private_key);
    assert(bytes_equal(decrypted1, plaintext));
    assert(ch->queue.empty() && "Queue should be empty after recv");

    // ——— Phase 2: Immediate handoff with listen() ———
    lattice_listen(DST);

    auto cipher2 = pq::kyber::encrypt(plaintext, kp.public_key);
    send_msg.m1_i1() = static_cast<int>(cipher2.size());
    send_msg.m1_p1() = reinterpret_cast<char *>(cipher2.data());

    assert(lattice_send(SRC, DST, send_msg) == OK);
    assert(g_graph.inbox_.find(DST) != g_graph.inbox_.end() &&
           "Inbox should contain direct message");

    message recv2{};
    assert(lattice_recv(DST, &recv2) == OK);
    std::span<const std::byte> recv_bytes2{reinterpret_cast<const std::byte *>(recv2.m1_p1()),
                                           static_cast<std::size_t>(recv2.m1_i1())};
    auto decrypted2 = pq::kyber::decrypt(recv_bytes2, kp.private_key);
    assert(bytes_equal(decrypted2, plaintext));

    // ——— Phase 3: No further messages ———
    message none{};
    int res = lattice_recv(DST, &none);
    assert(res == static_cast<int>(ErrorCode::E_NO_MESSAGE));

    return 0;
}

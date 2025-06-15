/**
 * @file test_lattice_ipc.cpp
 * @brief Verify lattice IPC message delivery and PQ encryption.
 */

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/const.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "kyber.hpp"

#include <cassert>
#include <span>
#include <string_view>
#include <vector>

using namespace lattice;

/**
 * @brief Convert a textual message to a byte vector.
 *
 * @param text Input string view.
 * @return Vector containing the bytes of @p text.
 */
static std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> result(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        result[i] = std::byte{static_cast<unsigned char>(text[i])};
    }
    return result;
}

/**
 * @brief Check equality of two byte spans.
 *
 * @param a First span.
 * @param b Second span.
 * @return True when both spans hold identical byte sequences.
 */
static bool bytes_equal(std::span<const std::byte> a, std::span<const std::byte> b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

/**
 * @brief Exercise lattice IPC primitives together with encryption.
 *
 * The test sends an encrypted payload between two synthetic processes and
 * verifies queued delivery and direct handoff. After each receive the
 * ciphertext is decrypted and compared against the original plaintext.
 *
 * @return 0 on success.
 */
int main() {
    g_graph = Graph{}; // reset global state

    constexpr int SRC = 40;
    constexpr int DST = 41;

    assert(lattice_connect(SRC, DST) == OK);
    Channel *ch = g_graph.find(SRC, DST);
    assert(ch != nullptr);

    const std::string_view text = "lattice secret";
    const auto plaintext = to_bytes(text);
    const auto kp = pq::kyber::keypair();
    auto cipher = pq::kyber::encrypt(plaintext, kp.public_key);

    message send{};
    send.m_type = 1;
    send.m1_i1() = static_cast<int>(cipher.size());
    send.m1_p1() = reinterpret_cast<char *>(cipher.data());

    assert(lattice_send(SRC, DST, send) == OK); // queued delivery
    assert(!ch->queue.empty());

    message recv{};
    assert(lattice_recv(DST, &recv) == OK);
    std::span<const std::byte> rx{reinterpret_cast<const std::byte *>(recv.m1_p1()),
                                  static_cast<std::size_t>(recv.m1_i1())};
    auto plain = pq::kyber::decrypt(rx, kp.private_key);
    assert(bytes_equal(plain, plaintext));
    assert(ch->queue.empty());

    lattice_listen(DST); // immediate handoff
    auto cipher2 = pq::kyber::encrypt(plaintext, kp.public_key);
    send.m1_i1() = static_cast<int>(cipher2.size());
    send.m1_p1() = reinterpret_cast<char *>(cipher2.data());
    assert(lattice_send(SRC, DST, send) == OK);
    assert(g_graph.inbox.find(DST) != g_graph.inbox.end());

    message recv2{};
    assert(lattice_recv(DST, &recv2) == OK);
    std::span<const std::byte> rx2{reinterpret_cast<const std::byte *>(recv2.m1_p1()),
                                   static_cast<std::size_t>(recv2.m1_i1())};
    auto plain2 = pq::kyber::decrypt(rx2, kp.private_key);
    assert(bytes_equal(plain2, plaintext));

    message none{};
    int res = lattice_recv(DST, &none);
    assert(res == static_cast<int>(ErrorCode::E_NO_MESSAGE));

    return 0;
}

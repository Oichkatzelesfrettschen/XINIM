/**
 * @file test_lattice.cpp
 * @brief Unit tests for lattice IPC primitives.
 */

#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"
#include "../kernel/pqcrypto.hpp"
#include <algorithm>
#include <cassert>

/**
 * @brief Validate queuing behavior and PQ secret negotiation.
 */
int main() {
    using namespace lattice;

    g_graph = Graph{}; // reset global state

    message msg{};
    msg.m_type = 42;

    assert(lattice_connect(1, 2) == OK);
    assert(g_graph.find(1, 2, net::local_node()) != nullptr);

    // Destination not listening -> message enqueued
    assert(lattice_send(1, 2, msg) == OK);
    auto *ch = g_graph.find(1, 2, net::local_node());
    assert(ch && ch->queue.size() == 1);

    message out{};
    assert(lattice_recv(2, &out) == OK);
    assert(out.m_type == 42);
    assert(ch->queue.empty());

    // Listening destination -> direct handoff
    lattice_listen(2);
    msg.m_type = 99;
    assert(lattice_send(1, 2, msg) == OK);
    assert(g_graph.inbox_.find(2) != g_graph.inbox_.end());
    message out2{};
    assert(lattice_recv(2, &out2) == OK);
    assert(out2.m_type == 99);

    /**
     * @brief Validate that computing a shared secret yields
     *        a non-zero 32-byte value.
     */
    auto a = pqcrypto::generate_keypair();
    auto b = pqcrypto::generate_keypair();
    auto secret = pqcrypto::compute_shared_secret(a, b);

    assert(secret.size() == pqcrystals_kyber512_BYTES);
    bool nonzero = std::any_of(secret.begin(), secret.end(), [](std::uint8_t b) { return b != 0; });
    assert(nonzero && "Secret must contain entropy");

    return 0;
}

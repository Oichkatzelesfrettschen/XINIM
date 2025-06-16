/**
 * @file test_lattice_network.cpp
 * @brief Exercise lattice IPC across simulated network nodes.
 */

#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

using namespace lattice;

/**
 * @brief XOR cipher identical to the implementation inside lattice_ipc.cpp.
 */
static void xor_cipher(std::span<std::byte> buf, std::span<const std::byte> key) noexcept {
    for (std::size_t i = 0; i < buf.size(); ++i) {
        buf[i] ^= key[i % key.size()];
    }
}

/**
 * @brief Deliver network bytes into the given graph instance.
 *
 * Packets are decrypted when the destination is listening otherwise they are
 * queued for later receipt.
 */
static void deliver(lattice::Graph &g, int node, xinim::pid_t src, xinim::pid_t dst) {
    auto data = net::receive(node);
    assert(!data.empty());
    assert(data.size() == sizeof(message));

    message msg{};
    std::memcpy(&msg, data.data(), sizeof(msg));

    lattice::Channel *ch = g.find(src, dst, net::local_node());
    assert(ch != nullptr);

    if (g.is_listening(dst)) {
        auto buf = std::span<std::byte>(reinterpret_cast<std::byte *>(&msg), sizeof(msg));
        xor_cipher(
            buf, std::span<const std::byte>(reinterpret_cast<const std::byte *>(ch->secret.data()),
                                            ch->secret.size()));
        g.inbox_[dst] = msg;
        g.set_listening(dst, false);
    } else {
        ch->queue.push_back(msg);
    }
}

/**
 * @brief Entry point verifying networked lattice IPC semantics.
 */
int main() {
    net::reset();

    lattice::Graph node0{};
    lattice::Graph node1{};

    // Establish matching channels on both nodes
    g_graph = node0;
    lattice_connect(10, 20, 1);
    const auto secret_a0 = g_graph.find(10, 20, 1)->secret;
    node0 = g_graph;

    g_graph = node1;
    lattice_connect(10, 20);
    lattice::Channel *a1 = g_graph.find(10, 20, net::local_node());
    a1->secret = secret_a0;
    node1 = g_graph;

    // Second channel to verify unique secrets
    g_graph = node0;
    lattice_connect(11, 22, 1);
    const auto secret_b0 = g_graph.find(11, 22, 1)->secret;
    node0 = g_graph;

    g_graph = node1;
    lattice_connect(11, 22);
    lattice::Channel *b1 = g_graph.find(11, 22, net::local_node());
    b1->secret = secret_b0;
    node1 = g_graph;

    assert(secret_a0 != secret_b0);

    // Phase 1: queued receive
    g_graph = node0;
    message m1{};
    m1.m_type = 42;
    lattice_send(10, 20, m1);
    node0 = g_graph;

    g_graph = node1;
    deliver(g_graph, 1, 10, 20);
    node1 = g_graph;

    message out1{};
    assert(lattice_recv(20, &out1) == OK);
    assert(out1.m_type == 42);
    node1 = g_graph;

    // Phase 2: listening path
    lattice_listen(20);
    node1 = g_graph;

    g_graph = node0;
    message m2{};
    m2.m_type = 99;
    lattice_send(10, 20, m2);
    node0 = g_graph;

    g_graph = node1;
    deliver(g_graph, 1, 10, 20);
    node1 = g_graph;

    message out2{};
    assert(lattice_recv(20, &out2) == OK);
    assert(out2.m_type == 99);

    return 0;
}

/**
 * @file test_lattice_corrupt_packet.cpp
 * @brief Ensure corrupted encrypted messages are rejected.
 */

#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/lattice_ipc.hpp"

#include <cassert>

using namespace lattice;

int main() {
    g_graph = Graph{};
    lattice_connect(1, 2);

    message msg{};
    msg.m_type = 55;
    assert(lattice_send(1, 2, msg) == OK);

    Channel *ch = g_graph.find(1, 2);
    assert(ch && !ch->queue.empty());

    ch->queue.front().data[0] ^= std::byte{0xFF};

    message out{};
    int rc = lattice_recv(2, &out);
    assert(rc == static_cast<int>(ErrorCode::E_NO_MESSAGE));
    assert(ch->queue.empty());
    return 0;
}

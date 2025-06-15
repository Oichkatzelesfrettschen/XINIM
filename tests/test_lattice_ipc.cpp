/**
 * @file test_lattice_ipc.cpp
 * @brief Unit test exercising lattice IPC send/receive behavior.
 */

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/const.hpp"
#include "../kernel/lattice_ipc.hpp"
#include <cassert>

/**
 * @brief Entry point for the lattice IPC unit test.
 *
 * The test establishes a channel between two synthetic processes and checks
 * queued delivery, immediate delivery when listening, and the error code when
 * no message is available.
 *
 * @return 0 on success, non-zero otherwise.
 */
int main() {
    using namespace lattice;

    // Reset global graph to a clean state.
    g_graph = Graph{};

    constexpr int SRC_PID = 11;
    constexpr int DST_PID = 22;

    // Establish the channel between the processes.
    assert(lattice_connect(SRC_PID, DST_PID) == OK);
    Channel *ch = g_graph.find(SRC_PID, DST_PID);
    assert(ch != nullptr);

    message msg{};
    msg.m_type = 7;

    // Destination not listening: message should be queued.
    assert(lattice_send(SRC_PID, DST_PID, msg) == OK);
    assert(!ch->queue.empty());

    message out{};
    // Retrieve the queued message.
    assert(lattice_recv(DST_PID, &out) == OK);
    assert(out.m_type == 7);
    assert(ch->queue.empty());

    // Mark receiver as listening and send again.
    lattice_listen(DST_PID);
    msg.m_type = 9;
    assert(lattice_send(SRC_PID, DST_PID, msg) == OK);
    assert(g_graph.inbox.find(DST_PID) != g_graph.inbox.end());

    message out2{};
    assert(lattice_recv(DST_PID, &out2) == OK);
    assert(out2.m_type == 9);

    // No message should yield E_NO_MESSAGE and keep listening state.
    message dummy{};
    int res = lattice_recv(DST_PID, &dummy);
    assert(res == static_cast<int>(ErrorCode::E_NO_MESSAGE));

    return 0;
}

/**
 * @file test_lattice_send_recv.cpp
 * @brief Comprehensive unit tests for lattice_send and lattice_recv.
 *
 * The tests validate direct message handoff with scheduler yielding,
 * queued delivery semantics when the receiver is not listening, and
 * non-blocking failure modes for both send and receive operations.
 */

#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/schedule.hpp"
#include <cassert>

using namespace lattice;

/**
 * @brief Verify that sending to a listening process yields control to it.
 */
static void test_direct_delivery() {
    using sched::scheduler;

    g_graph = Graph{};
    scheduler = sched::Scheduler{};

    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt(); // current = 1

    lattice_connect(1, 2);
    lattice_listen(2);

    message msg{};
    msg.m_type = 7;

    assert(scheduler.current() == 1);
    assert(lattice_send(1, 2, msg) == OK);
    assert(scheduler.current() == 2);
    assert(g_graph.inbox_.contains(2));

    message out{};
    assert(lattice_recv(2, &out) == OK);
    assert(out.m_type == 7);
}

/**
 * @brief Ensure messages queue when the destination is not listening.
 */
static void test_queued_delivery() {
    using sched::scheduler;

    g_graph = Graph{};
    scheduler = sched::Scheduler{};

    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt(); // current = 1

    lattice_connect(1, 2);

    message msg{};
    msg.m_type = 8;

    assert(lattice_send(1, 2, msg) == OK);
    assert(scheduler.current() == 1);

    Channel *ch = g_graph.find(1, 2);
    assert(ch && ch->queue.size() == 1);

    message out{};
    assert(lattice_recv(2, &out) == OK);
    assert(out.m_type == 8);
    assert(ch->queue.empty());
}

/**
 * @brief Validate non-blocking failure cases for send and recv.
 */
static void test_nonblocking_failures() {
    using sched::scheduler;

    g_graph = Graph{};
    scheduler = sched::Scheduler{};

    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt(); // current = 1

    lattice_connect(1, 2);

    message msg{};
    msg.m_type = 9;

    int rc = lattice_send(1, 2, msg, IpcFlags::NONBLOCK);
    assert(rc == static_cast<int>(ErrorCode::E_TRY_AGAIN));
    Channel *ch = g_graph.find(1, 2);
    assert(ch && ch->queue.empty());
    assert(scheduler.current() == 1);

    message out{};
    rc = lattice_recv(2, &out, IpcFlags::NONBLOCK);
    assert(rc == static_cast<int>(ErrorCode::E_NO_MESSAGE));
}

/**
 * @brief Entry point executing all lattice IPC tests.
 */
int main() {
    test_direct_delivery();
    test_queued_delivery();
    test_nonblocking_failures();
    return 0;
}

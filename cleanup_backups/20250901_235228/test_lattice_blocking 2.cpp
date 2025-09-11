/**
 * @file test_lattice_blocking.cpp
 * @brief Regression tests for blocking IPC semantics.
 */

#include "../h/error.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/schedule.hpp"

#include <cassert>
#include <chrono>
#include <thread>

using namespace lattice;
using namespace std::chrono_literals;

/**
 * @brief Thread entry waiting for a message.
 *
 * The function simply calls lattice_recv and stores the result code.
 */
static void receiver_task(int *rc, message *out) { *rc = lattice_recv(2, out); }

int main() {
    using sched::scheduler;

    g_graph = Graph{};
    scheduler = sched::Scheduler{};

    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt(); // current = 1

    lattice_connect(1, 2);

    message out{};
    int result = 0;
    std::thread receiver(receiver_task, &result, &out);
    std::this_thread::sleep_for(10ms);
    assert(scheduler.is_blocked(2));

    message msg{};
    msg.m_type = 77;
    assert(lattice_send(1, 2, msg) == OK);

    receiver.join();
    assert(result == OK);
    assert(out.m_type == 77);
    assert(!scheduler.is_blocked(2));

    return 0;
}

/**
 * @file test_fastpath_preconditions.cpp
 * @brief Verify fastpath precondition enforcement and message region handling.
 */

#include "../kernel/schedule.hpp"
#include "../kernel/wormhole.hpp"
#include <cassert>

using namespace fastpath;

/**
 * @brief Ensure execute_fastpath fails when preconditions are violated.
 */

/**
 * @brief Entry point verifying fastpath precondition checks.
 *
 * The function configures a scheduler state with two threads and sets up a
 * valid message region. It intentionally violates the P1 precondition by
 * setting @c extra_caps to a non-zero value, expecting @c execute_fastpath to
 * fail. Successful execution returns @c 0.
 *
 * @return Exit status code.
 */
int main() {
    using sched::scheduler;
    State s{};

    // Set up scheduler state with two runnable threads.
    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt();

    // Configure an aligned zero-copy message region.
    alignas(64) uint64_t buf[2]{};
    MessageRegion region(reinterpret_cast<std::uintptr_t>(buf), sizeof(buf));
    set_message_region(s, region);
    assert(message_region_valid(region, 1));

    // Populate sender and receiver threads.
    s.sender.tid = 1;
    s.sender.status = ThreadStatus::Running;
    s.sender.priority = 1;
    s.sender.domain = 0;
    s.sender.core = 0;
    s.receiver.tid = 2;
    s.receiver.status = ThreadStatus::RecvBlocked;
    s.receiver.priority = 1;
    s.receiver.domain = 0;
    s.receiver.core = 0;

    // Valid endpoint and capability setup.
    s.endpoint.eid = 1;
    s.endpoint.state = EndpointState::Recv;
    s.endpoint.queue.push_back(2);
    s.cap.cptr = 1;
    s.cap.type = CapType::Endpoint;
    s.cap.rights.write = true;
    s.cap.object = 1;
    s.cap.badge = 7;

    s.msg_len = 1;
    s.extra_caps = 1; // violate P1
    s.current_tid = scheduler.current();

    FastpathStats stats;
    bool ok = execute_fastpath(s, &stats);
    assert(!ok);
    assert(stats.failure_count == 1);
    auto idx = static_cast<size_t>(Precondition::P1);
    assert(stats.precondition_failures[idx] == 1);
    assert(scheduler.current() == s.current_tid);

    // Validate message_region_valid rejects regions that are too small.
    MessageRegion small_region(reinterpret_cast<std::uintptr_t>(buf), sizeof(uint64_t));
    assert(!message_region_valid(small_region, 2));

    return 0;
}

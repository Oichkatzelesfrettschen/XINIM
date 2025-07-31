#include "../kernel/schedule.hpp"
#include "../kernel/wormhole.hpp"
#include <cassert>

using namespace fastpath;

/**
 * @brief Verify fallback to shared memory when the per-CPU queue is full.
 */
int main() {
    using sched::scheduler;
    State s{};
    reset_fastpath_queues();
    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt();
    alignas(64) uint64_t buffer[8]{};
    set_message_region(s, MessageRegion(reinterpret_cast<std::uintptr_t>(buffer), sizeof(buffer)));

    s.sender.tid = 1;
    s.sender.priority = 5;
    s.sender.domain = 0;
    s.sender.core = 0;
    s.sender.mrs[0] = 99;
    s.msg_len = 1;
    s.extra_caps = 0;

    s.receiver.tid = 2;
    s.receiver.priority = 5;
    s.receiver.domain = 0;
    s.receiver.core = 0;

    s.endpoint.eid = 1;
    s.cap.cptr = 1;
    s.cap.type = CapType::Endpoint;
    s.cap.rights.write = true;
    s.cap.object = 1;

    s.current_tid = scheduler.current();
    FastpathStats stats;

    for (std::size_t i = 0; i < FASTPATH_QUEUE_SIZE; ++i) {
        s.sender.status = ThreadStatus::Running;
        s.receiver.status = ThreadStatus::RecvBlocked;
        s.endpoint.state = EndpointState::Recv;
        s.endpoint.queue = {2};
        bool ok = execute_fastpath(s, &stats);
        assert(ok);
        scheduler.enqueue(1);
        scheduler.enqueue(2);
        scheduler.preempt();
    }

    s.sender.status = ThreadStatus::Running;
    s.receiver.status = ThreadStatus::RecvBlocked;
    s.endpoint.state = EndpointState::Recv;
    s.endpoint.queue = {2};
    bool ok = execute_fastpath(s, &stats);
    assert(ok);
    assert(stats.fallback_count == 1);
    assert(stats.hit_count == FASTPATH_QUEUE_SIZE);
    return 0; //!< success
}

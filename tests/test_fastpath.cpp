#include "../kernel/schedule.hpp"
#include "../kernel/wormhole.hpp"
#include <cassert>

using namespace fastpath;

// Simple sanity test of the fastpath partial function with zero-copy buffer.
int main() {
    using sched::scheduler;
    State s{};
    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt();
    alignas(64) uint64_t buffer[8]{};
    set_message_region(s, MessageRegion(reinterpret_cast<std::uintptr_t>(buffer), sizeof(buffer)));
    s.sender.tid = 1;
    s.sender.status = ThreadStatus::Running;
    s.sender.priority = 5;
    s.sender.domain = 0;
    s.sender.core = 0;
    s.sender.badge = 0;
    s.sender.reply_to = 0;
    s.sender.mrs[0] = 42;
    s.msg_len = 1;
    s.extra_caps = 0;

    s.receiver.tid = 2;
    s.receiver.status = ThreadStatus::RecvBlocked;
    s.receiver.priority = 5;
    s.receiver.domain = 0;
    s.receiver.core = 0;
    s.receiver.badge = 0;
    s.receiver.reply_to = 0;

    s.endpoint.eid = 1;
    s.endpoint.state = EndpointState::Recv;
    s.endpoint.queue.push_back(2);

    s.cap.cptr = 1;
    s.cap.type = CapType::Endpoint;
    s.cap.rights.write = true;
    s.cap.object = 1;
    s.cap.badge = 123;

    s.current_tid = scheduler.current();

    FastpathStats stats; // collect fastpath metrics
    bool ok = execute_fastpath(s, &stats);
    assert(ok);
    assert(stats.success_count == 1);
    assert(s.receiver.mrs[0] == 42);
    assert(buffer[0] == 42);
    assert(s.receiver.status == ThreadStatus::Running);
    assert(s.sender.status == ThreadStatus::Blocked);
    assert(s.receiver.badge == s.cap.badge);
    assert(s.sender.reply_to == s.receiver.tid);
    assert(scheduler.current() == s.receiver.tid);
    assert(s.current_tid == s.receiver.tid);
    return 0;
}

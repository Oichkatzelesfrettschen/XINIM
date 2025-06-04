#include "../kernel/wormhole.hpp"
#include <cassert>

using namespace fastpath;

// Simple sanity test of the fastpath partial function.
int main() {
    State s{};
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

    s.current_tid = s.sender.tid;

    FastpathStats stats; // collect fastpath metrics
    bool ok = execute_fastpath(s, &stats);
    assert(ok);
    assert(stats.success_count == 1);
    assert(s.receiver.mrs[0] == 42);
    assert(s.receiver.status == ThreadStatus::Running);
    assert(s.sender.status == ThreadStatus::Blocked);
    assert(s.receiver.badge == s.cap.badge);
    assert(s.sender.reply_to == s.receiver.tid);
    assert(s.current_tid == s.receiver.tid);
    return 0;
}

#include "fastpath.hpp"

#include <algorithm>

namespace fastpath {

// Helper to check Send right presence.
static bool has_send_right(const CapRights &r) {
    return r.write; // treat write as send for this model
}

/* Fastpath partial function implementing the formalized state transition.
 * Returns true when all preconditions are satisfied and the fastpath is
 * executed. On failure the state remains unchanged and false is returned.
 */
bool execute_fastpath(State &s, FastpathStats *stats /*= nullptr*/) {
    // Helper lambda to update statistics.
    auto check_precondition = [&](bool cond, size_t idx) {
        if (!cond) {
            if (stats) {
                stats->failure_count.fetch_add(1, std::memory_order_relaxed);
                stats->precondition_failures[idx].fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }
        return true;
    };

    // Preconditions from P1..P9
    if (!check_precondition(s.extra_caps == 0, 0)) {
        return false; // P1
    }
    if (!check_precondition(s.msg_len <= s.sender.mrs.size(), 1)) {
        return false; // P2
    }
    if (!check_precondition(!s.sender.fault.has_value(), 2)) {
        return false; // P3
    }
    if (!check_precondition(s.cap.type == CapType::Endpoint && has_send_right(s.cap.rights), 3)) {
        return false; // P4
    }
    if (!check_precondition(s.endpoint.state == EndpointState::Recv && !s.endpoint.queue.empty(),
                            4)) {
        return false; // P5
    }
    if (!check_precondition(s.receiver.priority >= s.sender.priority, 5)) {
        return false; // P6 simplified
    }
    if (!check_precondition(s.sender.domain == s.receiver.domain, 6)) {
        return false; // P7 simplified
    }
    // P8 is not modeled; record as always satisfied
    if (!check_precondition(true, 7)) {
        return false; // P8 placeholder
    }
    if (!check_precondition(s.sender.core == s.receiver.core, 8)) {
        return false; // P9 simplified
    }

    // Dequeue receiver from endpoint
    auto it = std::find(s.endpoint.queue.begin(), s.endpoint.queue.end(), s.receiver.tid);
    if (it != s.endpoint.queue.end()) {
        s.endpoint.queue.erase(it);
    }
    if (s.endpoint.queue.empty()) {
        s.endpoint.state = EndpointState::Idle;
    }

    // Transfer message registers
    for (size_t i = 0; i < s.msg_len && i < s.sender.mrs.size(); ++i) {
        s.receiver.mrs[i] = s.sender.mrs[i];
    }

    // Update thread states

    s.receiver.status = ThreadStatus::Running;
    s.sender.status = ThreadStatus::Blocked;

    if (stats) {
        stats->success_count.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

} // namespace fastpath

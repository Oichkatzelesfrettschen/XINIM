#include "fastpath.hpp"

#include <algorithm>

namespace fastpath {

namespace detail {

// Remove the receiver from the endpoint queue and update its state.
void dequeue_receiver(State &s) {
    auto it = std::find(s.endpoint.queue.begin(), s.endpoint.queue.end(), s.receiver.tid);
    if (it != s.endpoint.queue.end()) {
        s.endpoint.queue.erase(it);
    }
    if (s.endpoint.queue.empty()) {
        s.endpoint.state = EndpointState::Idle;
    }
}

// Copy capability badge to the receiver thread.
void transfer_badge(State &s) { s.receiver.badge = s.cap.badge; }

// Establish reply linkage from sender to receiver.
void establish_reply(State &s) { s.sender.reply_to = s.receiver.tid; }

// Move message registers from sender to receiver.
void copy_mrs(State &s) {
    for (size_t i = 0; i < s.msg_len && i < s.sender.mrs.size(); ++i) {
        s.receiver.mrs[i] = s.sender.mrs[i];
    }
}

// Update scheduling state after IPC.
void update_thread_state(State &s) {
    s.receiver.status = ThreadStatus::Running;
    s.sender.status = ThreadStatus::Blocked;
}

// Simulate context switch to the receiver.
void context_switch(State &s) { s.current_tid = s.receiver.tid; }

} // namespace detail

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
    auto check_precondition = [&](bool cond, Precondition idx) {
        if (!cond) {
            if (stats) {
                stats->failure_count.fetch_add(1, std::memory_order_relaxed);
                stats->precondition_failures[static_cast<size_t>(idx)].fetch_add(
                    1, std::memory_order_relaxed);
            }
            return false;
        }
        return true;
    };

    // Preconditions from P1..P9
    if (!check_precondition(s.extra_caps == 0, Precondition::P1)) {
        return false; // P1
    }
    if (!check_precondition(s.msg_len <= s.sender.mrs.size(), Precondition::P2)) {
        return false; // P2
    }
    if (!check_precondition(!s.sender.fault.has_value(), Precondition::P3)) {
        return false; // P3
    }
    if (!check_precondition(s.cap.type == CapType::Endpoint && has_send_right(s.cap.rights),
                            Precondition::P4)) {
        return false; // P4
    }
    if (!check_precondition(s.endpoint.state == EndpointState::Recv && !s.endpoint.queue.empty(),
                            Precondition::P5)) {
        return false; // P5
    }
    if (!check_precondition(s.receiver.priority >= s.sender.priority, Precondition::P6)) {
        return false; // P6 simplified
    }
    if (!check_precondition(s.sender.domain == s.receiver.domain, Precondition::P7)) {
        return false; // P7 simplified
    }
    // P8 is not modeled; record as always satisfied
    if (!check_precondition(true, Precondition::P8)) {
        return false; // P8 placeholder
    }
    if (!check_precondition(s.sender.core == s.receiver.core, Precondition::P9)) {
        return false; // P9 simplified
    }

    // Atomic state updates composing the fastpath transition
    detail::dequeue_receiver(s);
    detail::transfer_badge(s);
    detail::establish_reply(s);
    detail::copy_mrs(s);
    detail::update_thread_state(s);
    detail::context_switch(s);

    if (stats) {
        stats->success_count.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

} // namespace fastpath

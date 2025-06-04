#include "wormhole.hpp"

// Implementation of the fastpath "wormhole" IPC model used for
// unit testing.  The code is intentionally simplified but mirrors the
// semantics of the real kernel fastpath.

#include <algorithm>
#include <cassert>

namespace fastpath {

// Set the zero-copy message region for fastpath execution.  Compile-time
// checks guarantee that the provided region type supports zero-copy
// mappings.
void set_message_region(State &state, const MessageRegion &region) {
    static_assert(MessageRegion::traits::is_zero_copy_capable,
                  "MessageRegion must support zero-copy");
    assert(region.aligned());
    state.msg_region = region;
}

// Verify that the message region can hold the requested message length.
bool message_region_valid(const MessageRegion &region, size_t msg_len) {
    return region.size() >= msg_len * sizeof(uint64_t) && region.aligned();
}

namespace detail {

// --\tau_dequeue-- remove receiver from endpoint queue and adjust
// endpoint state.  When the queue becomes empty the endpoint returns to
// the Idle state.
inline void dequeue_receiver(State &state) {
    auto it =
        std::find(state.endpoint.queue.begin(), state.endpoint.queue.end(), state.receiver.tid);
    if (it != state.endpoint.queue.end()) {
        state.endpoint.queue.erase(it);
    }
    if (state.endpoint.queue.empty()) {
        state.endpoint.state = EndpointState::Idle;
    }
}

// --\tau_badge-- deliver badge from capability to receiver.
inline void transfer_badge(State &state) { state.receiver.badge = state.cap.badge; }

// --\tau_reply-- set up reply linkage from sender to receiver.
inline void establish_reply(State &state) { state.sender.reply_to = state.receiver.tid; }

// --\tau_mrs-- copy message registers from sender to receiver
inline void copy_mrs(State &state) {
    // Ensure the provided message region is large and aligned enough.
    assert(message_region_valid(state.msg_region, state.msg_len));

    // Use zero_copy_map to access the region without additional copies.
    auto *buffer = static_cast<uint64_t *>(state.msg_region.zero_copy_map());

    // Copy from sender to the shared region then into the receiver registers.
    for (size_t i = 0; i < state.msg_len && i < state.sender.mrs.size(); ++i) {
        buffer[i] = state.sender.mrs[i];
        state.receiver.mrs[i] = buffer[i];
    }
}

// --\tau_state-- update scheduling state after IPC.  The receiver
// becomes runnable while the sender blocks waiting for a reply.
inline void update_thread_state(State &state) {
    state.receiver.status = ThreadStatus::Running;
    state.sender.status = ThreadStatus::Blocked;
}

// --\tau_switch-- context switch to the receiver thread.
inline void context_switch(State &state) { state.current_tid = state.receiver.tid; }

} // namespace detail

// Helper to determine if a capability conveys send rights.
static bool has_send_right(const CapRights &rights) { return rights.write; }

// Helper for updating statistics when a precondition check fails.
static bool check(bool condition, Precondition idx, FastpathStats *stats) {
    if (condition) {
        return true;
    }
    if (stats != nullptr) {
        stats->failure_count.fetch_add(1, std::memory_order_relaxed);
        stats->precondition_failures[static_cast<size_t>(idx)].fetch_add(1,
                                                                         std::memory_order_relaxed);
    }
    return false;
}

// Evaluate all preconditions for a fastpath execution attempt.
static bool preconditions(const State &s, FastpathStats *stats) {
    return check(s.extra_caps == 0, Precondition::P1, stats) &&
           check(s.msg_len <= s.sender.mrs.size(), Precondition::P2, stats) &&
           check(!s.sender.fault.has_value(), Precondition::P3, stats) &&
           check(s.cap.type == CapType::Endpoint && has_send_right(s.cap.rights), Precondition::P4,
                 stats) &&
           check(s.endpoint.state == EndpointState::Recv && !s.endpoint.queue.empty(),
                 Precondition::P5, stats) &&
           check(s.receiver.priority >= s.sender.priority, Precondition::P6, stats) &&
           check(s.sender.domain == s.receiver.domain, Precondition::P7, stats) &&
           check(true, Precondition::P8, stats) && // placeholder
           check(s.sender.core == s.receiver.core, Precondition::P9, stats);
}

// convenient alias for transformation function pointer
using Transformer = void (*)(State &);

// Main fastpath entry: apply all transformation steps when the
// preconditions hold.
bool execute_fastpath(State &state, FastpathStats *stats) {
    if (!preconditions(state, stats)) {
        return false;
    }

    constexpr Transformer steps[] = {detail::dequeue_receiver,    detail::transfer_badge,
                                     detail::establish_reply,     detail::copy_mrs,
                                     detail::update_thread_state, detail::context_switch};

    for (Transformer step : steps) {
        step(state);
    }

    if (stats != nullptr) {
        stats->success_count.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

} // namespace fastpath

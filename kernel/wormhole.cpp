#include "wormhole.hpp"
#include "schedule.hpp"

/**
 * @file wormhole.cpp
 * @brief Simplified fastpath implementation for unit tests.
 */
#include <algorithm>
#include <atomic> // For std::memory_order_relaxed (used in check)
#include <cassert>
#include <cstdint>    // For uint64_t
#include <functional> // For std::function or function pointers if Transformer were more complex
#include <vector>     // For std::vector::erase (used in dequeue_receiver)

namespace fastpath {

// Moved set_message_region and message_region_valid to the outer fastpath namespace
/// Set the region used for message transfer.
///
/// @param state Fastpath state to modify.
/// @param region Memory region describing the message buffer.
/// @return void
void set_message_region(State &state, const MessageRegion &region) noexcept {
    static_assert(MessageRegion::traits::is_zero_copy_capable,
                  "MessageRegion must support zero-copy");
    assert(region.aligned());
    state.msg_region = region;
}

/// Determine whether a message region can hold a given number of words.
///
/// @param region Memory region to validate.
/// @param msg_len Number of message words required.
/// @return True if the region is suitably sized and aligned.
bool message_region_valid(const MessageRegion &region, size_t msg_len) noexcept {
    return region.size() >= msg_len * sizeof(uint64_t) && region.aligned();
}

namespace detail {

// --\tau_dequeue-- remove receiver from endpoint queue and adjust endpoint state

// Implementation of the fastpath "wormhole" IPC model used for
// unit testing.  The code is intentionally simplified but mirrors the
// semantics of the real kernel fastpath.

// Removed duplicate inner "namespace fastpath {"

// namespace detail { // This was the beginning of the second, nested detail. Consolidating.

/// Remove the receiver from the endpoint queue and update endpoint state.
///
/// @param state Fastpath state holding endpoint data.
/// @return void
inline void dequeue_receiver(State &state) noexcept {
    std::erase(state.endpoint.queue, state.receiver.tid);
    if (state.endpoint.queue.empty()) {
        state.endpoint.state = EndpointState::Idle;
    }
}

/// Deliver the sender's badge to the receiver.
///
/// @param state Fastpath state referencing sender and receiver.
/// @return void
inline void transfer_badge(State &state) noexcept { state.receiver.badge = state.cap.badge; }

/// Establish reply linkage from sender to receiver.
///
/// @param state Fastpath state referencing sender and receiver.
/// @return void
inline void establish_reply(State &state) noexcept { state.sender.reply_to = state.receiver.tid; }

/// Copy message registers from sender to receiver.
///
/// @param state Fastpath state containing message buffers.
/// @return void
inline void copy_mrs(State &state) noexcept {
    const auto len = std::min(state.msg_len, state.sender.mrs.size());
    if (message_region_valid(state.msg_region, state.msg_len)) {
        auto *buffer = static_cast<uint64_t *>(state.msg_region.zero_copy_map());
        std::ranges::copy_n(state.sender.mrs.begin(), len, buffer);
        std::ranges::copy_n(buffer, len, state.receiver.mrs.begin());
    } else {
        std::ranges::copy_n(state.sender.mrs.begin(), len, state.receiver.mrs.begin());
    }
}

// } // End of the original inner "namespace detail"

// These functions are now part of the consolidated "namespace detail"
/// Update scheduling state after IPC.
/// The receiver becomes runnable while the sender blocks waiting for a reply.
///
/// @param state Fastpath state referencing threads.
/// @return void
inline void update_thread_state(State &state) noexcept {
    state.receiver.status = ThreadStatus::Running;
    state.sender.status = ThreadStatus::Blocked;
}

/// Context switch to the receiver thread using the global scheduler.
///
/// @param state Fastpath state being updated.
/// @return void
inline void context_switch(State &state) noexcept {
    sched::scheduler.yield_to(state.receiver.tid);
    state.current_tid = sched::scheduler.current();
}

} // namespace detail

// These static functions remain in the outer ::fastpath namespace
// update statistics for a failed precondition
// Helper to determine if a capability conveys send rights.
/// Determine whether the given rights include send permission.
///
/// @param rights Capability rights to inspect.
/// @return True if send is permitted.
static bool has_send_right(const CapRights &rights) noexcept { return rights.write; }

// Helper for updating statistics when a precondition check fails.
/// Record a failed precondition when @p condition is false.
///
/// @param condition Expression result to evaluate.
/// @param idx Index of the failing precondition.
/// @param stats Optional statistics structure to update.
/// @return True if @p condition is true.
static bool check(bool condition, Precondition idx, FastpathStats *stats) noexcept {
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
/// Verify that the current state satisfies all fastpath requirements.
///
/// @param s State to validate.
/// @param stats Optional statistics structure for failure accounting.
/// @return True if every precondition holds.
static bool preconditions(const State &s, FastpathStats *stats) noexcept {
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

/// Apply all transformation steps when fastpath preconditions hold.
///
/// @param state Fastpath state describing sender and receiver.
/// @param stats Optional statistics structure to update.
/// @return True if the fastpath completed successfully.
bool execute_fastpath(State &state, FastpathStats *stats) noexcept {
    if (!preconditions(state, stats)) {
        return false;
    }

    constexpr std::array<Transformer, 6> steps{detail::dequeue_receiver,    detail::transfer_badge,
                                               detail::establish_reply,     detail::copy_mrs,
                                               detail::update_thread_state, detail::context_switch};

    std::ranges::for_each(steps, [&state](Transformer step) { step(state); });

    if (stats != nullptr) {
        stats->success_count.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

} // namespace fastpath

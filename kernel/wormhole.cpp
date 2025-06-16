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

std::array<PerCpuQueue, NR_CPUS> cpu_queues{};

void reset_fastpath_queues() noexcept {
    for (auto &q : cpu_queues) {
        q.used = 0;
    }
}

// Moved set_message_region and message_region_valid to the outer fastpath namespace
/**
 * @brief Store a zero-copy message region.
 *
 * The region must meet MessageRegion alignment requirements.
 *
 * @param state  Fastpath state to modify.
 * @param region Message buffer region.
 */
void set_message_region(State &state, const MessageRegion &region) noexcept {
    static_assert(MessageRegion::traits::is_zero_copy_capable,
                  "MessageRegion must support zero-copy");
    assert(region.aligned());
    state.msg_region = region;
}

/**
 * @brief Check if a region can hold @p msg_len message words.
 *
 * @param region Memory region to validate.
 * @param msg_len Number of message words required.
 * @return @c true when the region is large and aligned enough.
 */
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

/**
 * @brief Remove the receiver from the endpoint queue.
 *
 * Marks the endpoint idle when the queue becomes empty.
 *
 * @param state Fastpath state holding endpoint data.
 */
inline void dequeue_receiver(State &state) noexcept {
    std::erase(state.endpoint.queue, state.receiver.tid);
    if (state.endpoint.queue.empty()) {
        state.endpoint.state = EndpointState::Idle;
    }
}

/**
 * @brief Deliver the sender's badge to the receiver.
 *
 * @param state Fastpath state referencing both threads.
 */
inline void transfer_badge(State &state) noexcept { state.receiver.badge = state.cap.badge; }

/**
 * @brief Link the sender to the receiver for replies.
 *
 * @param state Fastpath state referencing both threads.
 */
inline void establish_reply(State &state) noexcept { state.sender.reply_to = state.receiver.tid; }

/**
 * @brief Copy message registers from sender to receiver.
 *
 * The function first attempts to store the registers in the per-CPU
 * fastpath queue. When the queue is full, it spills the data to the
 * shared message region.
 *
 * @param state Fastpath state containing the message buffers.
 * @param stats Optional statistics tracker updated with hit/fallback counts.
 * @param cache Optional intermediate buffer selected by the caller.
 */
inline void copy_mrs(State &state, FastpathStats *stats,
                     const MessageRegion *cache = nullptr) noexcept {
    auto &queue = cpu_queues[state.sender.core];
    const auto len = std::min(state.msg_len, state.sender.mrs.size());
    if (!queue.full()) {
        auto &slot = queue.slots[queue.used];
        std::ranges::copy_n(state.sender.mrs.begin(), len, slot.begin());
        queue.lengths[queue.used] = len;
        ++queue.used;
        std::ranges::copy_n(slot.begin(), len, state.receiver.mrs.begin());
        if (stats != nullptr) {
            stats->hit_count.fetch_add(1, std::memory_order_relaxed);
        }
    } else {
        const MessageRegion *region = nullptr;
        if (cache != nullptr && message_region_valid(*cache, state.msg_len)) {
            region = cache;
        } else if (message_region_valid(state.msg_region, state.msg_len)) {
            region = &state.msg_region;
        }

        if (region != nullptr) {
            auto *buffer = static_cast<uint64_t *>(region->zero_copy_map());
            std::ranges::copy_n(state.sender.mrs.begin(), len, buffer);
            std::ranges::copy_n(buffer, len, state.receiver.mrs.begin());
        } else {
            std::ranges::copy_n(state.sender.mrs.begin(), len, state.receiver.mrs.begin());
        }
        if (stats != nullptr) {
            stats->fallback_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// } // End of the original inner "namespace detail"

// These functions are now part of the consolidated "namespace detail"
/**
 * @brief Update thread states after IPC.
 *
 * The receiver runs while the sender blocks waiting for a reply.
 *
 * @param state Fastpath state referencing both threads.
 */
inline void update_thread_state(State &state) noexcept {
    state.receiver.status = ThreadStatus::Running;
    state.sender.status = ThreadStatus::Blocked;
}

/**
 * @brief Switch execution to the receiver thread.
 *
 * @param state Fastpath state being updated.
 */
inline void context_switch(State &state) noexcept {
    sched::scheduler.yield_to(state.receiver.tid);
    state.current_tid = sched::scheduler.current();
}

} // namespace detail

// These static functions remain in the outer ::fastpath namespace
// update statistics for a failed precondition
// Helper to determine if a capability conveys send rights.
/**
 * @brief Determine whether the given rights include send permission.
 *
 * @param rights Capability rights to inspect.
 * @return @c true if send is permitted.
 */
static bool has_send_right(const CapRights &rights) noexcept { return rights.write; }

// Helper for updating statistics when a precondition check fails.
/**
 * @brief Record a failed precondition when @p condition is false.
 *
 * @param condition Expression result to evaluate.
 * @param idx Index of the failing precondition.
 * @param stats Optional statistics structure to update.
 * @return @c true when the condition holds.
 */
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
/**
 * @brief Verify that the current state satisfies all fastpath requirements.
 *
 * @param s State to validate.
 * @param stats Optional statistics structure for failure accounting.
 * @return @c true if every precondition holds.
 */
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

/**
 * @brief Apply all transformation steps when preconditions hold.
 *
 * @param state Fastpath state describing sender and receiver.
 * @param stats Optional statistics structure to update.
 * @return @c true if the fastpath completed successfully.
 */
bool execute_fastpath(State &state, FastpathStats *stats) noexcept {
    if (!preconditions(state, stats)) {
        return false;
    }

    detail::dequeue_receiver(state);
    detail::transfer_badge(state);
    detail::establish_reply(state);

    // Select the smallest valid cache before spilling to main memory.
    const MessageRegion *selected = nullptr;
    if (message_region_valid(state.l1_buffer, state.msg_len)) {
        selected = &state.l1_buffer;
    } else if (message_region_valid(state.l2_buffer, state.msg_len)) {
        selected = &state.l2_buffer;
    } else if (message_region_valid(state.l3_buffer, state.msg_len)) {
        selected = &state.l3_buffer;
    }

    detail::copy_mrs(state, stats, selected);
    detail::update_thread_state(state);
    detail::context_switch(state);

    if (stats != nullptr) {
        stats->success_count.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

} // namespace fastpath

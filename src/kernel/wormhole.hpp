#pragma once

#include "../include/psd/vm/semantic_memory.hpp"
#include "const.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

namespace fastpath {

// Alias simplifying access to message semantic region type.  The
// wormhole relies on a zero-copy capable message region to move
// data between threads efficiently.
using MessageRegion = psd::vm::semantic_region<psd::vm::semantic_message_tag>;

/// Number of zero-copy queue slots per CPU.
inline constexpr std::size_t FASTPATH_QUEUE_SIZE = 4;

/**
 * @brief Per-CPU queue retaining recently transferred messages.
 *
 * Each slot keeps one message copy aligned for cache residency.
 */
struct PerCpuQueue {
    /// Cached message storage.
    alignas(64) std::array<std::array<uint64_t, 8>, FASTPATH_QUEUE_SIZE> slots{};
    /// Length of the message stored per slot.
    std::array<std::size_t, FASTPATH_QUEUE_SIZE> lengths{};
    /// Number of populated slots.
    std::size_t used{0};

    /// Determine whether no slots remain.
    [[nodiscard]] bool full() const noexcept { return used >= FASTPATH_QUEUE_SIZE; }
};

/// Per-CPU fastpath queues.
extern std::array<PerCpuQueue, NR_CPUS> cpu_queues;

/**
 * @brief Reset the per-CPU fastpath queues.
 *
 * Tests call this helper to start with empty caches ensuring
 * predictable hit/fallback statistics.
 */
void reset_fastpath_queues() noexcept;
// State components as described in the formalization.

// Possible thread execution states.
// Thread scheduling states.
enum class ThreadStatus { Running, Blocked, SendBlocked, RecvBlocked };

// Receive endpoint state.
// Endpoint operational mode.
enum class EndpointState { Idle, Send, Recv };

// Capability type enumeration.
// Kernel object type described by the capability.
enum class CapType { Endpoint };

// Basic capability rights bitmask.
struct CapRights {
    bool read{false};
    bool write{false};
    bool grant{false};
    bool grant_reply{false};
};

// Representation of a capability object.
struct Capability {
    uint32_t cptr{}; // capability pointer
    CapType type{CapType::Endpoint};
    CapRights rights{}; // access rights
    uint32_t object{};  // referenced kernel object id
    uint32_t badge{};   // badge value delivered to receiver
};

// Thread template provides configurable message register count.  In the
// actual kernel MR_COUNT is architecture dependent.  Here we expose it
// for testing and flexibility.
template <size_t MR_COUNT = 8> struct ThreadTemplate {
    uint32_t tid{};                             // thread identifier
    ThreadStatus status{ThreadStatus::Blocked}; // scheduler state
    uint8_t priority{};                         // scheduling priority
    uint16_t domain{};                          // domain identifier
    uint32_t vspace{};                          // address space reference
    std::optional<int> fault;                   // fault number, if any
    uint8_t core{};                             // CPU affinity
    uint32_t badge{};                           // delivered badge value
    uint32_t reply_to{};                        // thread id to reply to
    std::array<uint64_t, MR_COUNT> mrs{};       // message registers

    // Safe accessor returning nullopt when out of range.
    std::optional<uint64_t> get_mr(size_t index) const {
        return index < mrs.size() ? std::optional<uint64_t>{mrs[index]} : std::nullopt;
    }
};

// Default thread type using eight message registers.
using Thread = ThreadTemplate<>;

// Endpoint structure holding a queue of waiting threads.  Only one
// endpoint is modeled for this simplified fastpath implementation.

struct Endpoint {
    uint32_t eid{};              // endpoint identifier
    std::vector<uint32_t> queue; // queued thread ids
    EndpointState state{EndpointState::Idle};
};

// Complete system state used by the fastpath.  Tests construct this
// structure directly to model kernel behavior during IPC.
struct State {
    Thread sender;       // sending thread
    Thread receiver;     // receiving thread
    Endpoint endpoint;   // communication endpoint
    Capability cap;      // capability used by sender
    size_t msg_len{};    // message length
    size_t extra_caps{}; // number of extra caps
    MessageRegion msg_region{0, 0};
    // region used for zero-copy transfer of message registers

    /**
     * @brief First-level cache for message copies.
     *
     * Aligned region used when the per-CPU queue overflows but remains
     * within a single core. The buffer is optional and considered invalid
     * when its size is zero.
     */
    MessageRegion l1_buffer{0, 0};

    /**
     * @brief Second-level cache for inter-core traffic.
     *
     * Messages spill here when the L1 buffer is full.  The same validity
     * rules apply as for :c member:`l1_buffer`.
     */
    MessageRegion l2_buffer{0, 0};

    /**
     * @brief Third-level cache shared among all cores.
     *
     * This buffer is used only when both L1 and L2 caches are exhausted.
     */
    MessageRegion l3_buffer{0, 0};

    uint32_t current_tid{}; // currently running thread id
};

// Statistics collected from fastpath execution.
// Preconditions enumerated for statistic indexing.
enum class Precondition : size_t { P1, P2, P3, P4, P5, P6, P7, P8, P9, Count };

// Statistics collected from fastpath execution.  They allow tests to
// confirm that each precondition is checked correctly.
struct FastpathStats {
    std::atomic<uint64_t> success_count{0}; // completed runs
    std::atomic<uint64_t> failure_count{0}; // failed runs
    std::array<std::atomic<uint64_t>, static_cast<size_t>(Precondition::Count)>
        precondition_failures{};             // counters per precondition
    std::atomic<uint64_t> hit_count{0};      ///< queue fastpath hits
    std::atomic<uint64_t> fallback_count{0}; ///< spill to shared memory

    FastpathStats() {
        for (auto &counter : precondition_failures) {
            counter.store(0, std::memory_order_relaxed);
        }
    }
};

/**
 * @brief Execute the fastpath transformations if preconditions are met.
 *
 * Applies each transformation step to @p state and updates @p stats when
 * provided.
 *
 * @param state Mutable fastpath state.
 * @param stats Optional statistics collector.
 * @return @c true when all preconditions pass and the fastpath completed.
 */
bool execute_fastpath(State &state, FastpathStats *stats = nullptr) noexcept;

/**
 * @brief Configure a zero-copy message region for the fastpath.
 *
 * The region must satisfy the alignment requirements defined by
 * MessageRegion.
 *
 * @param state  Fastpath state to modify.
 * @param region Zero-copy message region.
 */
void set_message_region(State &state, const MessageRegion &region) noexcept;

/**
 * @brief Validate that a message region can hold @p msg_len registers.
 *
 * @param region Region to validate.
 * @param msg_len Number of message registers required.
 * @return @c true if the region is large and aligned enough.
 */
bool message_region_valid(const MessageRegion &region, size_t msg_len) noexcept;

/**
 * @brief Choose the highest priority cache able to hold the message.
 *
 * The function checks the three cache levels stored in @p state. The
 * first region large enough for the message is returned. A @c nullptr
 * indicates that none of the caches are usable for this message.
 *
 * @param state State containing the cache descriptors.
 * @return Pointer to the chosen cache or @c nullptr when no cache fits.
 */
[[nodiscard]] const MessageRegion *select_cache(const State &state) noexcept;

namespace detail {
/// Remove the receiver from the endpoint queue.
void dequeue_receiver(State &state) noexcept;
/// Transfer the sender's badge to the receiver.
void transfer_badge(State &state) noexcept;
/// Link the sender to the receiver for replies.
void establish_reply(State &state) noexcept;
/// Copy message registers using the configured message region.
void copy_mrs(State &state, FastpathStats *stats) noexcept;
/// Update thread states after IPC.
void update_thread_state(State &state) noexcept;
/// Switch execution to the receiver.
void context_switch(State &state) noexcept;
} // namespace detail

} // namespace fastpath

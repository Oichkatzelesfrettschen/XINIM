#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

namespace fastpath {

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

// Thread template provides configurable message register count.
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

// Endpoint structure holding a queue of waiting threads.
struct Endpoint {
    uint32_t eid{};              // endpoint identifier
    std::vector<uint32_t> queue; // queued thread ids
    EndpointState state{EndpointState::Idle};
};

// Complete system state used by the fastpath.
struct State {
    Thread sender;          // sending thread
    Thread receiver;        // receiving thread
    Endpoint endpoint;      // communication endpoint
    Capability cap;         // capability used by sender
    size_t msg_len{};       // message length
    size_t extra_caps{};    // number of extra caps
    uint32_t current_tid{}; // currently running thread id
};

// Statistics collected from fastpath execution.
// Preconditions enumerated for statistic indexing.
enum class Precondition : size_t { P1, P2, P3, P4, P5, P6, P7, P8, P9, Count };

// Statistics collected from fastpath execution.
struct FastpathStats {
    std::atomic<uint64_t> success_count{0}; // completed runs
    std::atomic<uint64_t> failure_count{0}; // failed runs
    std::array<std::atomic<uint64_t>, static_cast<size_t>(Precondition::Count)>
        precondition_failures{}; // counters per precondition

    FastpathStats() {
        for (auto &counter : precondition_failures) {
            counter.store(0, std::memory_order_relaxed);
        }
    }
};

// Fastpath operation modeled as a partial function on State.
bool execute_fastpath(State &s, FastpathStats *stats = nullptr);

namespace detail {
void dequeue_receiver(State &s);
void transfer_badge(State &s);
void establish_reply(State &s);
void copy_mrs(State &s);
void update_thread_state(State &s);
void context_switch(State &s);
} // namespace detail

} // namespace fastpath

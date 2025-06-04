#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

namespace fastpath {

// State components as described in the formalization.

// Possible thread execution states.
enum class ThreadStatus { Running, Blocked, SendBlocked, RecvBlocked };

// Receive endpoint state.
enum class EndpointState { Idle, Send, Recv };

// Capability type enumeration.
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
    Thread sender;       // sending thread
    Thread receiver;     // receiving thread
    Endpoint endpoint;   // communication endpoint
    Capability cap;      // capability used by sender
    size_t msg_len{};    // message length
    size_t extra_caps{}; // number of extra caps
};

// Statistics collected from fastpath execution.
struct FastpathStats {
    std::atomic<uint64_t> success_count{0};                       // completed runs
    std::atomic<uint64_t> failure_count{0};                       // failed runs
    std::array<std::atomic<uint64_t>, 9> precondition_failures{}; // P1-P9

    FastpathStats() {
        for (auto &counter : precondition_failures) {
            counter.store(0, std::memory_order_relaxed);
        }
    }
};

// Fastpath operation modeled as a partial function on State.
bool execute_fastpath(State &s, FastpathStats *stats = nullptr);

} // namespace fastpath

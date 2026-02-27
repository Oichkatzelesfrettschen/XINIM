#pragma once
/**
 * @file capability_mutex.hpp
 * @brief Capability-based mutex (Fixed-size wait queue).
 */

#include "../include/xinim/core_types.hpp"
#include "schedule.hpp"
#include "octonion.hpp"
#include "fano_octonion.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace xinim::sync {

struct CapabilityToken {
    uint64_t token_id;
    xinim::pid_t issuer_pid;
    uint64_t expiry_time;
    uint32_t rights;
    lattice::Octonion proof;

    static constexpr uint32_t RIGHT_READ = 0x01;
    static constexpr uint32_t RIGHT_WRITE = 0x02;
    
    [[nodiscard]] bool has_right(uint32_t right) const noexcept {
        return (rights & right) != 0;
    }
};

class CapabilityMutex {
  public:
    static constexpr std::size_t MAX_WAITERS = 16;

    explicit CapabilityMutex() : owner_(-1), locked_(false) {}

    bool lock(xinim::pid_t pid, const CapabilityToken& token) noexcept;
    void unlock(xinim::pid_t pid) noexcept;
    void force_unlock() noexcept;

    [[nodiscard]] bool is_locked() const noexcept { return locked_; }
    [[nodiscard]] xinim::pid_t owner() const noexcept { return owner_; }

  private:
    xinim::pid_t owner_;
    std::atomic<bool> locked_;
    
    struct Waiter {
        xinim::pid_t pid;
        uint64_t token_id;
    };
    Waiter wait_queue_[MAX_WAITERS]{};
    std::size_t wait_count_{0};

    bool verify_token(const CapabilityToken& token) const noexcept;
};

} // namespace xinim::sync

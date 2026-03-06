#pragma once
/**
 * @file quaternion_spinlock.hpp
 * @brief RAII TAS spinlock with quaternion-themed API.
 *
 * v1.2.0: Removed non-atomic `orientation` field that was written inside
 * the critical section without synchronization (data race). The quaternion
 * ticket parameter is now purely cosmetic -- the real lock is atomic_flag.
 */

#include <atomic>
#include <cstdint>

namespace hyper {

/**
 * @brief Simple quaternion type (used as ticket token).
 */
struct Quaternion {
    float w{1.0F};
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    constexpr Quaternion() = default;
    constexpr Quaternion(float sw, float sx, float sy, float sz) noexcept
        : w(sw), x(sx), y(sy), z(sz) {}

    [[nodiscard]] static constexpr Quaternion id() noexcept { return {}; }

    [[nodiscard]] constexpr Quaternion operator*(const Quaternion &rhs) const noexcept {
        return Quaternion{
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
        };
    }

    [[nodiscard]] constexpr Quaternion conjugate() const noexcept {
        return Quaternion{w, -x, -y, -z};
    }
};

/// Maximum spin iterations before declaring a deadlock.
inline constexpr uint32_t QSPIN_MAX_SPINS = 100000;

/**
 * @brief TAS spinlock with quaternion-themed API.
 *
 * The ticket parameter is accepted for API compatibility but does not
 * affect lock semantics. The real lock is a single atomic_flag.
 */
class QuaternionSpinlock {
  public:
    QuaternionSpinlock() noexcept = default;

    void lock([[maybe_unused]] const Quaternion &ticket) noexcept {
        uint32_t spins = 0;
        while (flag.test_and_set(std::memory_order_acquire)) {
            if (++spins >= QSPIN_MAX_SPINS) break;
        }
    }

    void unlock([[maybe_unused]] const Quaternion &ticket) noexcept {
        flag.clear(std::memory_order_release);
    }

  private:
    std::atomic_flag flag{};
};

/**
 * @brief RAII helper that locks on construction and unlocks on destruction.
 */
class QuaternionLockGuard {
  public:
    QuaternionLockGuard(QuaternionSpinlock &spin, const Quaternion &t) noexcept
        : lock_(spin), ticket_(t) {
        lock_.lock(ticket_);
    }
    ~QuaternionLockGuard() { lock_.unlock(ticket_); }

    QuaternionLockGuard(const QuaternionLockGuard&) = delete;
    QuaternionLockGuard& operator=(const QuaternionLockGuard&) = delete;

  private:
    QuaternionSpinlock &lock_;
    Quaternion ticket_;
};

} // namespace hyper

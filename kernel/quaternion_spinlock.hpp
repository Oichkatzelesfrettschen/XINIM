#pragma once
/**
 * @file quaternion_spinlock.hpp
 * @brief RAII quaternion-based spinlock implementation.
 */

#include <atomic>
#include <cstdint>

namespace hyper {

/**
 * @brief Simple quaternion type.
 */
struct Quaternion {
    float w{1.0F}; ///< Scalar component
    float x{0.0F}; ///< i component
    float y{0.0F}; ///< j component
    float z{0.0F}; ///< k component

    constexpr Quaternion() = default;
    constexpr Quaternion(float sw, float sx, float sy, float sz) noexcept
        : w(sw), x(sx), y(sy), z(sz) {}

    /**
     * @brief Quaternion multiplication.
     */
    [[nodiscard]] constexpr Quaternion operator*(const Quaternion &rhs) const noexcept {
        return Quaternion{
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
        };
    }

    /**
     * @brief Conjugate quaternion.
     */
    [[nodiscard]] constexpr Quaternion conjugate() const noexcept {
        return Quaternion{w, -x, -y, -z};
    }
};

/**
 * @brief Spinlock using an atomic flag combined with quaternion state.
 */
class QuaternionSpinlock {
  public:
    QuaternionSpinlock() noexcept = default;

    /// Acquire the lock spinning until available.
    void lock() noexcept {
        while (flag.test_and_set(std::memory_order_acquire)) {
        }
        orientation = orientation * locked_quat;
    }

    /// Release the lock.
    void unlock() noexcept {
        orientation = orientation.conjugate();
        flag.clear(std::memory_order_release);
    }

  private:
    std::atomic_flag flag{};  ///< Lock flag
    Quaternion orientation{}; ///< Orientation state
    static constexpr Quaternion locked_quat{0.0F, 1.0F, 0.0F, 0.0F};
};

/**
 * @brief RAII helper that locks on construction and unlocks on destruction.
 */
class QuaternionLockGuard {
  public:
    explicit QuaternionLockGuard(QuaternionSpinlock &spin) noexcept : lock(spin) { lock.lock(); }

    ~QuaternionLockGuard() { lock.unlock(); }

  private:
    QuaternionSpinlock &lock; ///< Referenced spinlock
};

} // namespace hyper

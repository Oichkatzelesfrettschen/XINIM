#pragma once
/**
 * @file quaternion_spinlock.hpp
 * @brief RAII quaternion-based spinlock implementation.
 */

#include <atomic>
#include <cstdint> // For std::uintptr_t potentially if used with atomics, not directly here
#include <new>     // For std::hardware_destructive_interference_size

namespace hyper {

/**
 * @brief Simple quaternion type used by the spinlock.
 * Aligned to 16 bytes for SSE compatibility with 4 floats.
 */
struct alignas(16) Quaternion { // Added alignas(16)
    float w{1.0F}; ///< Scalar component
    float x{0.0F}; ///< i component
    float y{0.0F}; ///< j component
    float z{0.0F}; ///< k component

    /// Default to the identity element.
    constexpr Quaternion() = default;

    /// Initialize all components explicitly.
    constexpr Quaternion(float sw, float sx, float sy, float sz) noexcept
        : w(sw), x(sx), y(sy), z(sz) {}

    /// Obtain the identity quaternion.
    [[nodiscard]] static constexpr Quaternion id() noexcept { return {}; }

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
 * The internal `orientation` quaternion is used to manage fairness or ticket processing.
 * To prevent false sharing, ensure `orientation` and `flag` are on different cache lines if possible.
 */
class QuaternionSpinlock {
  public:
    QuaternionSpinlock() noexcept = default;

    /// Acquire the lock spinning until available.
    void lock(const Quaternion &ticket) noexcept {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Could add CPU pause instruction here (e.g., _mm_pause() on x86)
#if defined(__i386__) || defined(__x86_64__)
            __asm__ volatile("pause");
#endif
        }
        // Critical section for updating orientation (though brief)
        orientation = orientation * ticket;
    }

    /// Release the lock.
    void unlock(const Quaternion &ticket) noexcept {
        // Critical section for updating orientation (though brief)
        orientation = orientation * ticket.conjugate();
        flag.clear(std::memory_order_release);
    }

  private:
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 64
    // Align to cache line boundary to prevent false sharing with other data
    alignas(std::hardware_destructive_interference_size) std::atomic_flag flag{};
    alignas(std::hardware_destructive_interference_size) Quaternion orientation{};
#else
    // Fallback if hardware_destructive_interference_size is not available or too small
    // This might still lead to false sharing if Quaternion and atomic_flag are close in memory
    // and span a cache line boundary or share one. Padding might be an option here.
    // For now, let's add some padding manually if not using C++17 feature.
    // Assuming cache line size is 64 bytes.
    // sizeof(std::atomic_flag) is small (usually 1 byte). sizeof(Quaternion) is 16 bytes.
    std::atomic_flag flag{};
    char padding[64 - sizeof(std::atomic_flag) > 0 ? 64 - sizeof(std::atomic_flag) : 1]; // Ensure positive size
    Quaternion orientation{};
    char padding2[64 - sizeof(Quaternion) > 0 ? 64 - sizeof(Quaternion) : 1]; // Ensure positive size
#endif
};

/**
 * @brief RAII helper that locks on construction and unlocks on destruction.
 */
class QuaternionLockGuard {
  public:
    QuaternionLockGuard(QuaternionSpinlock &spin, const Quaternion &t) noexcept
        : lock_ref(spin), ticket_val(t) {
        lock_ref.lock(ticket_val);
    }
    ~QuaternionLockGuard() { lock_ref.unlock(ticket_val); }

    // Prevent copying and moving
    QuaternionLockGuard(const QuaternionLockGuard&) = delete;
    QuaternionLockGuard& operator=(const QuaternionLockGuard&) = delete;
    QuaternionLockGuard(QuaternionLockGuard&&) = delete;
    QuaternionLockGaurd& operator=(QuaternionLockGuard&&) = delete; // Typo Gaurd -> Guard

  private:
    QuaternionSpinlock &lock_ref; ///< Referenced spinlock
    const Quaternion ticket_val;  ///< Ticket quaternion used for this lock instance
};

} // namespace hyper

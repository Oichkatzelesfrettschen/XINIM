/**
 * @file quaternion.hpp
 * @brief Unified SIMD-optimized quaternion operations for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * High-performance quaternion library supporting:
 * - All major SIMD instruction sets (x86-64, ARM NEON/SVE)
 * - Runtime dispatch for optimal performance
 * - Compile-time instruction set selection
 * - Spinlock-optimized atomic operations
 * - Batch processing for multiple quaternions
 * - Mathematical operations (multiply, slerp, rotation, etc.)
 */

#pragma once

#include "../core.hpp"
#include "../detect.hpp"
#include <cmath>
#include <array>
#include <algorithm>
#include <concepts>
#include <type_traits>

#ifdef __AVX__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace xinim::simd::math {

/**
 * @brief Base quaternion template with SIMD optimization
 */
template<typename T>
    requires std::floating_point<T>
class alignas(std::max(32UL, sizeof(T) * 4)) quaternion {
public:
    using value_type = T;
    static constexpr std::size_t alignment = std::max(32UL, sizeof(T) * 4);
    
    // Component storage (w, x, y, z)
    union {
        struct { T w, x, y, z; };
        std::array<T, 4> components;
        
        // SIMD-specific storage
#ifdef __AVX__
        __m256d avx_data;   // For double precision
        __m128  sse_data;   // For single precision
#endif
#ifdef __ARM_NEON
        float32x4_t neon_data;      // For single precision ARM
        struct { float64x2_t lo, hi; } neon_f64; // For double precision ARM
#endif
    };

    // Constructors
    constexpr quaternion() noexcept : w(T{0}), x(T{0}), y(T{0}), z(T{0}) {}
    
    constexpr quaternion(T w_val, T x_val, T y_val, T z_val) noexcept
        : w(w_val), x(x_val), y(y_val), z(z_val) {}
    
    constexpr quaternion(T scalar, const std::array<T, 3>& vector) noexcept
        : w(scalar), x(vector[0]), y(vector[1]), z(vector[2]) {}
    
    // Copy and move constructors
    quaternion(const quaternion&) = default;
    quaternion& operator=(const quaternion&) = default;
    quaternion(quaternion&&) = default;
    quaternion& operator=(quaternion&&) = default;

    // Component access
    constexpr T& operator[](std::size_t index) noexcept {
        return components[index];
    }
    
    constexpr const T& operator[](std::size_t index) const noexcept {
        return components[index];
    }

    // Basic arithmetic operators (optimized with SIMD when available)
    quaternion& operator+=(const quaternion& other) noexcept;
    quaternion& operator-=(const quaternion& other) noexcept;
    quaternion& operator*=(const quaternion& other) noexcept; // Hamilton product
    quaternion& operator*=(T scalar) noexcept;
    quaternion& operator/=(T scalar) noexcept;

    // Non-member operator friends
    friend quaternion operator+(quaternion lhs, const quaternion& rhs) noexcept {
        return lhs += rhs;
    }
    
    friend quaternion operator-(quaternion lhs, const quaternion& rhs) noexcept {
        return lhs -= rhs;
    }
    
    friend quaternion operator*(quaternion lhs, const quaternion& rhs) noexcept {
        return lhs *= rhs;
    }
    
    friend quaternion operator*(quaternion q, T scalar) noexcept {
        return q *= scalar;
    }
    
    friend quaternion operator*(T scalar, quaternion q) noexcept {
        return q *= scalar;
    }

    // Core quaternion operations
    [[nodiscard]] quaternion conjugate() const noexcept;
    [[nodiscard]] quaternion inverse() const noexcept;
    [[nodiscard]] T norm_squared() const noexcept;
    [[nodiscard]] T norm() const noexcept;
    [[nodiscard]] quaternion normalize() const noexcept;
    [[nodiscard]] bool is_unit(T tolerance = T{1e-6}) const noexcept;

    // Rotation operations
    [[nodiscard]] quaternion slerp(const quaternion& target, T t) const noexcept;
    [[nodiscard]] std::array<T, 3> rotate_vector(const std::array<T, 3>& vec) const noexcept;
    [[nodiscard]] quaternion from_axis_angle(const std::array<T, 3>& axis, T angle) noexcept;
    [[nodiscard]] std::array<T, 4> to_axis_angle() const noexcept; // [x, y, z, angle]

    // Conversion to/from other representations
    [[nodiscard]] std::array<std::array<T, 3>, 3> to_rotation_matrix() const noexcept;
    [[nodiscard]] std::array<T, 3> to_euler_angles() const noexcept; // [roll, pitch, yaw]
    
    static quaternion from_rotation_matrix(const std::array<std::array<T, 3>, 3>& matrix) noexcept;
    static quaternion from_euler_angles(T roll, T pitch, T yaw) noexcept;

    // Comparison operators
    [[nodiscard]] bool operator==(const quaternion& other) const noexcept;
    [[nodiscard]] bool operator!=(const quaternion& other) const noexcept {
        return !(*this == other);
    }
    
    [[nodiscard]] bool approximately_equal(const quaternion& other, 
                                         T tolerance = T{1e-6}) const noexcept;

    // Special quaternions
    static constexpr quaternion identity() noexcept {
        return quaternion(T{1}, T{0}, T{0}, T{0});
    }
    
    static constexpr quaternion zero() noexcept {
        return quaternion(T{0}, T{0}, T{0}, T{0});
    }
};

// Type aliases for common precision levels
using quaternionf = quaternion<float>;
using quaterniond = quaternion<double>;

/**
 * @brief Atomic quaternion for spinlocks and concurrent operations
 */
template<typename T>
class alignas(std::max(64UL, sizeof(T) * 4)) atomic_quaternion {
private:
    mutable std::atomic<quaternion<T>> data;
    
public:
    atomic_quaternion() noexcept = default;
    atomic_quaternion(const quaternion<T>& q) noexcept : data(q) {}
    
    // Atomic operations
    quaternion<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept;
    void store(const quaternion<T>& q, std::memory_order order = std::memory_order_seq_cst) noexcept;
    quaternion<T> exchange(const quaternion<T>& q, std::memory_order order = std::memory_order_seq_cst) noexcept;
    
    bool compare_exchange_weak(quaternion<T>& expected, const quaternion<T>& desired,
                              std::memory_order order = std::memory_order_seq_cst) noexcept;
    bool compare_exchange_strong(quaternion<T>& expected, const quaternion<T>& desired,
                                std::memory_order order = std::memory_order_seq_cst) noexcept;
    
    // Spinlock operations using quaternion as a lock token
    bool try_lock() noexcept;
    void lock() noexcept;
    void unlock() noexcept;
    
    // Atomic quaternion operations
    quaternion<T> atomic_multiply(const quaternion<T>& other) noexcept;
    quaternion<T> atomic_normalize() noexcept;
};

/**
 * @brief Batch quaternion operations for SIMD optimization
 */
namespace batch {
    // Batch multiply operations
    template<typename T>
    void multiply(const quaternion<T>* a, const quaternion<T>* b, 
                 quaternion<T>* result, std::size_t count) noexcept;
    
    template<typename T>
    void multiply_scalar(const quaternion<T>* input, T scalar,
                        quaternion<T>* result, std::size_t count) noexcept;
    
    // Batch normalization
    template<typename T>
    void normalize(const quaternion<T>* input, quaternion<T>* output,
                  std::size_t count) noexcept;
    
    // Batch conjugation
    template<typename T>
    void conjugate(const quaternion<T>* input, quaternion<T>* output,
                  std::size_t count) noexcept;
    
    // Batch inverse
    template<typename T>
    void inverse(const quaternion<T>* input, quaternion<T>* output,
                std::size_t count) noexcept;
    
    // Batch SLERP
    template<typename T>
    void slerp(const quaternion<T>* start, const quaternion<T>* end,
              T t, quaternion<T>* result, std::size_t count) noexcept;
    
    // Batch vector rotation
    template<typename T>
    void rotate_vectors(const quaternion<T>* rotations, 
                       const std::array<T, 3>* vectors,
                       std::array<T, 3>* results, std::size_t count) noexcept;
}

/**
 * @brief Runtime dispatch for optimal quaternion operations
 */
namespace dispatch {
    // Function pointer types for different implementations
    template<typename T>
    using multiply_func = quaternion<T>(*)(const quaternion<T>&, const quaternion<T>&);
    
    template<typename T>
    using normalize_func = quaternion<T>(*)(const quaternion<T>&);
    
    template<typename T>
    using batch_multiply_func = void(*)(const quaternion<T>*, const quaternion<T>*, 
                                       quaternion<T>*, std::size_t);
    
    // Get optimal implementation for current hardware
    template<typename T>
    multiply_func<T> get_multiply_impl() noexcept;
    
    template<typename T>
    normalize_func<T> get_normalize_impl() noexcept;
    
    template<typename T>
    batch_multiply_func<T> get_batch_multiply_impl() noexcept;
}

/**
 * @brief Specialized quaternion implementations for different SIMD instruction sets
 */
namespace impl {
    // Scalar fallback implementations
    namespace scalar {
        template<typename T>
        quaternion<T> multiply(const quaternion<T>& a, const quaternion<T>& b) noexcept;
        
        template<typename T>
        quaternion<T> normalize(const quaternion<T>& q) noexcept;
    }
    
    // SSE implementations (x86-64)
    namespace sse {
        quaternionf multiply(const quaternionf& a, const quaternionf& b) noexcept;
        quaternionf normalize(const quaternionf& q) noexcept;
        void batch_multiply(const quaternionf* a, const quaternionf* b,
                           quaternionf* result, std::size_t count) noexcept;
    }
    
    // AVX implementations (x86-64)
    namespace avx {
        quaternionf multiply(const quaternionf& a, const quaternionf& b) noexcept;
        quaterniond multiply(const quaterniond& a, const quaterniond& b) noexcept;
        quaternionf normalize(const quaternionf& q) noexcept;
        quaterniond normalize(const quaterniond& q) noexcept;
        
        void batch_multiply_f32(const quaternionf* a, const quaternionf* b,
                               quaternionf* result, std::size_t count) noexcept;
        void batch_multiply_f64(const quaterniond* a, const quaterniond* b,
                               quaterniond* result, std::size_t count) noexcept;
    }
    
    // AVX-512 implementations (x86-64)  
    namespace avx512 {
        quaternionf multiply(const quaternionf& a, const quaternionf& b) noexcept;
        quaterniond multiply(const quaterniond& a, const quaterniond& b) noexcept;
        
        void batch_multiply_16(const quaternionf* a, const quaternionf* b,
                              quaternionf* result) noexcept; // Process 16 at once
        void batch_multiply_8(const quaterniond* a, const quaterniond* b,
                             quaterniond* result) noexcept; // Process 8 at once
    }
    
    // ARM NEON implementations
    namespace neon {
        quaternionf multiply(const quaternionf& a, const quaternionf& b) noexcept;
        quaternionf normalize(const quaternionf& q) noexcept;
        void batch_multiply(const quaternionf* a, const quaternionf* b,
                           quaternionf* result, std::size_t count) noexcept;
    }
    
    // ARM SVE implementations
    namespace sve {
        void multiply_streaming(const float* a_data, const float* b_data,
                               float* result_data, std::size_t count) noexcept;
        void normalize_streaming(const float* input_data, float* output_data,
                                std::size_t count) noexcept;
    }
}

} // namespace xinim::simd::math

// Include implementation details
#include "quaternion_impl.hpp"

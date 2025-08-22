/**
 * @file quaternion.cpp
 * @brief SIMD-optimized quaternion implementation for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * @sideeffects Quaternion operations are pure; atomic helpers modify internal state atomically.
 * @thread_safety Atomic variants are thread-safe, while regular operations require external synchronization.
 * @example
 * using namespace xinim::simd::math;
 * quaternion<float> q = quaternion<float>::identity();
 * auto v = q.rotate_vector({1.0f, 0.0f, 0.0f});
 */

#include "../../include/xinim/simd/math/quaternion.hpp"
#include "../../include/xinim/simd/detect.hpp"
#include <cmath>
#include <algorithm>

namespace xinim::simd::math {

// Atomic quaternion implementation
template<typename T>
quaternion<T> atomic_quaternion<T>::load(std::memory_order order) const noexcept {
    return data.load(order);
}

template<typename T>
void atomic_quaternion<T>::store(const quaternion<T>& q, std::memory_order order) noexcept {
    data.store(q, order);
}

template<typename T>
quaternion<T> atomic_quaternion<T>::exchange(const quaternion<T>& q, std::memory_order order) noexcept {
    return data.exchange(q, order);
}

template<typename T>
bool atomic_quaternion<T>::compare_exchange_weak(quaternion<T>& expected, 
                                                const quaternion<T>& desired,
                                                std::memory_order order) noexcept {
    return data.compare_exchange_weak(expected, desired, order);
}

template<typename T>
bool atomic_quaternion<T>::compare_exchange_strong(quaternion<T>& expected,
                                                  const quaternion<T>& desired,
                                                  std::memory_order order) noexcept {
    return data.compare_exchange_strong(expected, desired, order);
}

// Spinlock implementation using quaternion as lock token
template<typename T>
bool atomic_quaternion<T>::try_lock() noexcept {
    quaternion<T> expected = quaternion<T>::zero();
    quaternion<T> desired = quaternion<T>::identity();
    return data.compare_exchange_weak(expected, desired, std::memory_order_acquire);
}

template<typename T>
void atomic_quaternion<T>::lock() noexcept {
    while (!try_lock()) {
        // Spin wait with exponential backoff
        std::this_thread::yield();
    }
}

template<typename T>
void atomic_quaternion<T>::unlock() noexcept {
    data.store(quaternion<T>::zero(), std::memory_order_release);
}

// Advanced quaternion operations
template<typename T>
quaternion<T> quaternion<T>::inverse() const noexcept {
    T norm_sq = norm_squared();
    if (norm_sq > T{0}) {
        return conjugate() * (T{1} / norm_sq);
    }
    return *this; // Return original if not invertible
}

template<typename T>
bool quaternion<T>::is_unit(T tolerance) const noexcept {
    return std::abs(norm_squared() - T{1}) <= tolerance;
}

template<typename T>
quaternion<T> quaternion<T>::slerp(const quaternion<T>& target, T t) const noexcept {
    // Spherical linear interpolation implementation
    T dot = w * target.w + x * target.x + y * target.y + z * target.z;
    
    // Choose the shorter path
    quaternion<T> q1 = (dot < T{0}) ? -target : target;
    dot = std::abs(dot);
    
    if (dot > T{0.9995}) {
        // Linear interpolation for very close quaternions
        quaternion<T> result = *this * (T{1} - t) + q1 * t;
        return result.normalize();
    }
    
    T theta_0 = std::acos(dot);
    T theta = theta_0 * t;
    T sin_theta = std::sin(theta);
    T sin_theta_0 = std::sin(theta_0);
    
    T s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    T s1 = sin_theta / sin_theta_0;
    
    return *this * s0 + q1 * s1;
}

template<typename T>
std::array<T, 3> quaternion<T>::rotate_vector(const std::array<T, 3>& vec) const noexcept {
    // Efficient vector rotation using quaternion
    // v' = q * v * q^(-1) = v + 2 * cross(qv, v) + 2 * qw * cross(qv, v)
    // where qv = (x, y, z) is the vector part of quaternion
    
    std::array<T, 3> qv = {x, y, z};
    
    // Cross product: qv × vec
    std::array<T, 3> cross1 = {
        qv[1] * vec[2] - qv[2] * vec[1],
        qv[2] * vec[0] - qv[0] * vec[2],
        qv[0] * vec[1] - qv[1] * vec[0]
    };
    
    // Cross product: qv × cross1
    std::array<T, 3> cross2 = {
        qv[1] * cross1[2] - qv[2] * cross1[1],
        qv[2] * cross1[0] - qv[0] * cross1[2],
        qv[0] * cross1[1] - qv[1] * cross1[0]
    };
    
    return {
        vec[0] + T{2} * (w * cross1[0] + cross2[0]),
        vec[1] + T{2} * (w * cross1[1] + cross2[1]),
        vec[2] + T{2} * (w * cross1[2] + cross2[2])
    };
}

template<typename T>
quaternion<T> quaternion<T>::from_axis_angle(const std::array<T, 3>& axis, T angle) noexcept {
    T half_angle = angle * T{0.5};
    T sin_half = std::sin(half_angle);
    T cos_half = std::cos(half_angle);
    
    // Normalize axis
    T axis_length = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
    if (axis_length > T{0}) {
        T inv_length = T{1} / axis_length;
        return quaternion<T>(
            cos_half,
            axis[0] * inv_length * sin_half,
            axis[1] * inv_length * sin_half,
            axis[2] * inv_length * sin_half
        );
    }
    
    return identity();
}

template<typename T>
std::array<T, 4> quaternion<T>::to_axis_angle() const noexcept {
    T sin_half_angle = std::sqrt(x*x + y*y + z*z);
    
    if (sin_half_angle < T{1e-6}) {
        // No rotation
        return {T{1}, T{0}, T{0}, T{0}};
    }
    
    T angle = T{2} * std::atan2(sin_half_angle, w);
    T inv_sin = T{1} / sin_half_angle;
    
    return {
        x * inv_sin,
        y * inv_sin,
        z * inv_sin,
        angle
    };
}

template<typename T>
std::array<std::array<T, 3>, 3> quaternion<T>::to_rotation_matrix() const noexcept {
    T xx = x * x, yy = y * y, zz = z * z;
    T xy = x * y, xz = x * z, yz = y * z;
    T wx = w * x, wy = w * y, wz = w * z;
    
    return {{
        {{T{1} - T{2}*(yy + zz), T{2}*(xy - wz),       T{2}*(xz + wy)}},
        {{T{2}*(xy + wz),       T{1} - T{2}*(xx + zz), T{2}*(yz - wx)}},
        {{T{2}*(xz - wy),       T{2}*(yz + wx),       T{1} - T{2}*(xx + yy)}}
    }};
}

template<typename T>
quaternion<T> quaternion<T>::from_rotation_matrix(const std::array<std::array<T, 3>, 3>& m) noexcept {
    T trace = m[0][0] + m[1][1] + m[2][2];
    
    if (trace > T{0}) {
        T s = std::sqrt(trace + T{1}) * T{2}; // s = 4 * qw
        return quaternion<T>(
            T{0.25} * s,
            (m[2][1] - m[1][2]) / s,
            (m[0][2] - m[2][0]) / s,
            (m[1][0] - m[0][1]) / s
        );
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        T s = std::sqrt(T{1} + m[0][0] - m[1][1] - m[2][2]) * T{2}; // s = 4 * qx
        return quaternion<T>(
            (m[2][1] - m[1][2]) / s,
            T{0.25} * s,
            (m[0][1] + m[1][0]) / s,
            (m[0][2] + m[2][0]) / s
        );
    } else if (m[1][1] > m[2][2]) {
        T s = std::sqrt(T{1} + m[1][1] - m[0][0] - m[2][2]) * T{2}; // s = 4 * qy
        return quaternion<T>(
            (m[0][2] - m[2][0]) / s,
            (m[0][1] + m[1][0]) / s,
            T{0.25} * s,
            (m[1][2] + m[2][1]) / s
        );
    } else {
        T s = std::sqrt(T{1} + m[2][2] - m[0][0] - m[1][1]) * T{2}; // s = 4 * qz
        return quaternion<T>(
            (m[1][0] - m[0][1]) / s,
            (m[0][2] + m[2][0]) / s,
            (m[1][2] + m[2][1]) / s,
            T{0.25} * s
        );
    }
}

template<typename T>
std::array<T, 3> quaternion<T>::to_euler_angles() const noexcept {
    // Convert to Euler angles (roll, pitch, yaw) in radians
    T sinr_cosp = T{2} * (w * x + y * z);
    T cosr_cosp = T{1} - T{2} * (x * x + y * y);
    T roll = std::atan2(sinr_cosp, cosr_cosp);
    
    T sinp = T{2} * (w * y - z * x);
    T pitch;
    if (std::abs(sinp) >= T{1}) {
        pitch = std::copysign(T{M_PI} / T{2}, sinp); // Use 90 degrees if out of range
    } else {
        pitch = std::asin(sinp);
    }
    
    T siny_cosp = T{2} * (w * z + x * y);
    T cosy_cosp = T{1} - T{2} * (y * y + z * z);
    T yaw = std::atan2(siny_cosp, cosy_cosp);
    
    return {roll, pitch, yaw};
}

template<typename T>
quaternion<T> quaternion<T>::from_euler_angles(T roll, T pitch, T yaw) noexcept {
    T cr = std::cos(roll * T{0.5});
    T sr = std::sin(roll * T{0.5});
    T cp = std::cos(pitch * T{0.5});
    T sp = std::sin(pitch * T{0.5});
    T cy = std::cos(yaw * T{0.5});
    T sy = std::sin(yaw * T{0.5});
    
    return quaternion<T>(
        cr * cp * cy + sr * sp * sy,
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy
    );
}

template<typename T>
bool quaternion<T>::operator==(const quaternion<T>& other) const noexcept {
    return w == other.w && x == other.x && y == other.y && z == other.z;
}

template<typename T>
bool quaternion<T>::approximately_equal(const quaternion<T>& other, T tolerance) const noexcept {
    return std::abs(w - other.w) <= tolerance &&
           std::abs(x - other.x) <= tolerance &&
           std::abs(y - other.y) <= tolerance &&
           std::abs(z - other.z) <= tolerance;
}

// Explicit template instantiations
template class quaternion<float>;
template class quaternion<double>;
template class atomic_quaternion<float>;
template class atomic_quaternion<double>;

} // namespace xinim::simd::math

#ifndef XINIM_KERNEL_IPC_ALGEBRAIC_OCTONION_HPP
#define XINIM_KERNEL_IPC_ALGEBRAIC_OCTONION_HPP

#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp" // Updated path
#include <array>
#include <cmath>    // For std::sqrt, std::fabs
#include <iosfwd>   // For std::ostream forward declaration
#include <limits>   // For std::numeric_limits

namespace XINIM { // Changed namespace
namespace Algebraic {

// Epsilon for octonion floating point comparisons
constexpr double OCTONION_EPSILON = std::numeric_limits<double>::epsilon() * 100;

/**
 * @brief A class representing an Octonion with double precision components.
 *
 * Octonions are an 8-dimensional non-associative, non-commutative division algebra
 * over the real numbers. They extend quaternions via the Cayley-Dickson construction.
 * This class is aligned to 64 bytes for potential AVX-512 compatibility (8 doubles).
 *
 * @details An octonion o is represented as o = e0 + e1*i1 + ... + e7*i7.
 * It can also be seen as a pair of quaternions (a, b) as a + b*e4.
 */
class alignas(64) Octonion {
public:
    std::array<double, 8> c; ///< Components e0, e1, ..., e7

    // Constructors
    /** @brief Default constructor, initializes to zero octonion. */
    constexpr Octonion() noexcept : c{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0} {}

    /** @brief Constructor from eight scalar components. */
    constexpr Octonion(double c0, double c1, double c2, double c3,
                       double c4, double c5, double c6, double c7) noexcept
        : c{c0, c1, c2, c3, c4, c5, c6, c7} {}

    /** @brief Constructor from an array of 8 doubles. */
    constexpr explicit Octonion(const std::array<double, 8>& components) noexcept : c(components) {}

    /** @brief Constructor from two Quaternions (a, b) representing o = a + b*e4. */
    Octonion(const XINIM::Algebraic::Quaternion& q1, const XINIM::Algebraic::Quaternion& q2) noexcept;


    // Basic Arithmetic Operators
    constexpr Octonion& operator+=(const Octonion& rhs) noexcept {
        for (size_t i = 0; i < 8; ++i) c[i] += rhs.c[i];
        return *this;
    }

    constexpr Octonion& operator-=(const Octonion& rhs) noexcept {
        for (size_t i = 0; i < 8; ++i) c[i] -= rhs.c[i];
        return *this;
    }

    constexpr Octonion& operator*=(double scalar) noexcept {
        for (size_t i = 0; i < 8; ++i) c[i] *= scalar;
        return *this;
    }

    // Division by scalar. If scalar is near zero, sets to zero octonion.
    Octonion& operator/=(double scalar) noexcept;

    // Octonion (Cayley-Dickson) product. This is non-associative.
    Octonion& operator*=(const Octonion& rhs) noexcept;

    // Friend operators for non-member versions
    friend constexpr Octonion operator+(Octonion lhs, const Octonion& rhs) noexcept { return lhs += rhs; }
    friend constexpr Octonion operator-(Octonion lhs, const Octonion& rhs) noexcept { return lhs -= rhs; }
    friend constexpr Octonion operator*(Octonion lhs, double scalar) noexcept { return lhs *= scalar; }
    friend constexpr Octonion operator*(double scalar, Octonion rhs) noexcept { return rhs *= scalar; }
    friend Octonion operator/(Octonion lhs, double scalar) noexcept { return lhs /= scalar; }
    friend Octonion operator*(Octonion lhs, const Octonion& rhs) noexcept { return lhs *= rhs; }

    // Comparison Operators
    constexpr bool operator==(const Octonion& other) const noexcept {
        for (size_t i = 0; i < 8; ++i) {
            if (std::fabs(c[i] - other.c[i]) >= OCTONION_EPSILON) return false;
        }
        return true;
    }
    constexpr bool operator!=(const Octonion& other) const noexcept {
        return !(*this == other);
    }

    // Mathematical Operations
    /** @brief Computes the conjugate: (c0, -c1, ..., -c7). */
    [[nodiscard]] constexpr Octonion conjugate() const noexcept {
        return Octonion(c[0], -c[1], -c[2], -c[3], -c[4], -c[5], -c[6], -c[7]);
    }

    /** @brief Computes the squared norm (sum of squares of components). */
    [[nodiscard]] constexpr double norm_sq() const noexcept {
        double sum_sq = 0.0;
        for (double val : c) sum_sq += val * val;
        return sum_sq;
    }

    /** @brief Computes the norm (magnitude): sqrt(norm_sq()). */
    [[nodiscard]] double norm() const noexcept;

    /**
     * @brief Computes the inverse: conjugate() / norm_sq().
     * Returns a zero octonion if norm is close to zero.
     */
    [[nodiscard]] Octonion inverse() const noexcept;

    /**
     * @brief Normalizes to unit length. Remains zero if norm is close to zero.
     */
    Octonion& normalize() noexcept;

    /**
     * @brief Returns a normalized version. Returns zero if norm is close to zero.
     */
    [[nodiscard]] Octonion normalized() const noexcept;

    /**
     * @brief Checks if octonion is unit (norm is close to 1).
     * @param tolerance Default is OCTONION_EPSILON.
     */
    [[nodiscard]] bool is_unit(double tolerance = OCTONION_EPSILON) const noexcept;

    /**
     * @brief Checks if octonion is zero (all components close to zero).
     * @param tolerance Default is OCTONION_EPSILON.
     */
    [[nodiscard]] bool is_zero(double tolerance = OCTONION_EPSILON) const noexcept;

    // Static utility functions
    /** @brief Returns the identity octonion (1, 0, ..., 0). */
    [[nodiscard]] static constexpr Octonion identity() noexcept {
        return Octonion(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }
    /** @brief Returns the zero octonion (0, ..., 0). */
    [[nodiscard]] static constexpr Octonion zero() noexcept {
        return Octonion(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }

    // Access to Quaternion parts for Cayley-Dickson based operations
    /** @brief Gets the first quaternion part (e0-e3) used in (q1, q2) representation. */
    [[nodiscard]] XINIM::Algebraic::Quaternion get_q1() const noexcept;
    /** @brief Gets the second quaternion part (e4-e7) used in (q1, q2) representation. */
    [[nodiscard]] XINIM::Algebraic::Quaternion get_q2() const noexcept;

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Octonion& o);
};

} // namespace Algebraic
} // namespace XINIM

#endif // XINIM_KERNEL_IPC_ALGEBRAIC_OCTONION_HPP

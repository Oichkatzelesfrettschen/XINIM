#ifndef XINIM_KERNEL_IPC_ALGEBRAIC_QUATERNION_HPP
#define XINIM_KERNEL_IPC_ALGEBRAIC_QUATERNION_HPP

#include <array>
#include <cmath>   // For std::sqrt, std::fabs
#include <iosfwd>  // For std::ostream forward declaration
#include <limits>  // For std::numeric_limits

namespace XINIM { // Changed namespace
namespace Algebraic {

// A small epsilon value for floating point comparisons.
// Using sqrt of epsilon for tolerance in norm comparisons is sometimes done.
constexpr double QUATERNION_EPSILON = std::numeric_limits<double>::epsilon() * 100; // A bit larger than machine epsilon

/**
 * @brief A class representing a Quaternion with double precision components.
 *
 * Quaternions are used to represent rotations in 3D space and are elements of a
 * non-commutative division algebra. This class provides common mathematical operations.
 * It is aligned to 32 bytes for potential AVX compatibility (4 doubles).
 *
 * @details A quaternion q is represented as q = w + x*i + y*j + z*k,
 * where w is the scalar part and (x, y, z) form the vector part.
 * Components are named w, x, y, z for clarity (w=scalar, x,y,z=vector parts).
 */
class alignas(32) Quaternion {
public:
    double w; ///< Scalar part (real part, often 'r' or 's')
    double x; ///< First vector component (i part)
    double y; ///< Second vector component (j part)
    double z; ///< Third vector component (k part)

    // Constructors
    /** @brief Default constructor, initializes to zero quaternion (0,0,0,0). */
    constexpr Quaternion() noexcept : w(0.0), x(0.0), y(0.0), z(0.0) {}

    /** @brief Constructor from four scalar components. */
    constexpr Quaternion(double w_val, double x_val, double y_val, double z_val) noexcept
        : w(w_val), x(x_val), y(y_val), z(z_val) {}

    /** @brief Constructor from a scalar part and a 3D vector part (array). */
    constexpr Quaternion(double scalar_part, const std::array<double, 3>& vector_part) noexcept
        : w(scalar_part), x(vector_part[0]), y(vector_part[1]), z(vector_part[2]) {}

    // Basic Arithmetic Operators
    constexpr Quaternion& operator+=(const Quaternion& rhs) noexcept {
        w += rhs.w; x += rhs.x; y += rhs.y; z += rhs.z;
        return *this;
    }

    constexpr Quaternion& operator-=(const Quaternion& rhs) noexcept {
        w -= rhs.w; x -= rhs.x; y -= rhs.y; z -= rhs.z;
        return *this;
    }

    constexpr Quaternion& operator*=(double scalar) noexcept {
        w *= scalar; x *= scalar; y *= scalar; z *= scalar;
        return *this;
    }

    // Division by scalar. Returns *this. If scalar is near zero, behavior is to set to zero.
    Quaternion& operator/=(double scalar) noexcept;

    // Hamilton product
    Quaternion& operator*=(const Quaternion& rhs) noexcept;

    // Friend operators for non-member versions
    friend constexpr Quaternion operator+(Quaternion lhs, const Quaternion& rhs) noexcept {
        return lhs += rhs;
    }
    friend constexpr Quaternion operator-(Quaternion lhs, const Quaternion& rhs) noexcept {
        return lhs -= rhs;
    }
    friend constexpr Quaternion operator*(Quaternion lhs, double scalar) noexcept {
        return lhs *= scalar;
    }
    friend constexpr Quaternion operator*(double scalar, Quaternion rhs) noexcept {
        return rhs *= scalar;
    }
    friend Quaternion operator/(Quaternion lhs, double scalar) noexcept {
        return lhs /= scalar;
    }
    friend Quaternion operator*(Quaternion lhs, const Quaternion& rhs) noexcept {
        return lhs *= rhs;
    }

    // Comparison Operators
    constexpr bool operator==(const Quaternion& other) const noexcept {
        return (std::fabs(w - other.w) < QUATERNION_EPSILON &&
                std::fabs(x - other.x) < QUATERNION_EPSILON &&
                std::fabs(y - other.y) < QUATERNION_EPSILON &&
                std::fabs(z - other.z) < QUATERNION_EPSILON);
    }
    constexpr bool operator!=(const Quaternion& other) const noexcept {
        return !(*this == other);
    }

    // Mathematical Operations
    /** @brief Computes the conjugate of the quaternion: (w, -x, -y, -z). */
    [[nodiscard]] constexpr Quaternion conjugate() const noexcept {
        return Quaternion(w, -x, -y, -z);
    }

    /**
     * @brief Computes the squared norm (magnitude squared) of the quaternion.
     * This is w^2 + x^2 + y^2 + z^2. More efficient than norm() if only comparing magnitudes.
     */
    [[nodiscard]] constexpr double norm_sq() const noexcept {
        return w * w + x * x + y * y + z * z;
    }

    /** @brief Computes the norm (magnitude) of the quaternion: sqrt(norm_sq()). */
    [[nodiscard]] double norm() const noexcept;

    /**
     * @brief Computes the inverse of the quaternion: conjugate() / norm_sq().
     * Returns a zero quaternion if the original quaternion has a norm close to zero.
     */
    [[nodiscard]] Quaternion inverse() const noexcept;

    /**
     * @brief Normalizes the quaternion to unit length.
     * If the quaternion has a norm close to zero, it remains a zero quaternion.
     */
    Quaternion& normalize() noexcept;

    /**
     * @brief Returns a normalized version of this quaternion.
     * If the quaternion has a norm close to zero, a zero quaternion is returned.
     */
    [[nodiscard]] Quaternion normalized() const noexcept;

    /**
     * @brief Checks if the quaternion is a unit quaternion (norm is close to 1).
     * @param tolerance The tolerance for comparing the squared norm to 1.0. Default is QUATERNION_EPSILON.
     */
    [[nodiscard]] bool is_unit(double tolerance = QUATERNION_EPSILON) const noexcept;

    /**
     * @brief Checks if the quaternion is a zero quaternion (all components are close to zero).
     * @param tolerance The tolerance for comparing components to 0.0. Default is QUATERNION_EPSILON.
     */
    [[nodiscard]] bool is_zero(double tolerance = QUATERNION_EPSILON) const noexcept;


    // Static utility functions
    /** @brief Returns the identity quaternion (1, 0, 0, 0). */
    [[nodiscard]] static constexpr Quaternion identity() noexcept {
        return Quaternion(1.0, 0.0, 0.0, 0.0);
    }

    /** @brief Returns the zero quaternion (0, 0, 0, 0). */
    [[nodiscard]] static constexpr Quaternion zero() noexcept {
        return Quaternion(0.0, 0.0, 0.0, 0.0);
    }

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Quaternion& q);
};

} // namespace Algebraic
} // namespace XINIM

#endif // XINIM_KERNEL_IPC_ALGEBRAIC_QUATERNION_HPP

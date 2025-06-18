#ifndef COMMON_MATH_QUATERNION_HPP
#define COMMON_MATH_QUATERNION_HPP

#include <array>
#include <cmath> // For std::sqrt
#include <iosfwd> // For ostream declaration

namespace Common {
namespace Math {

/**
 * @brief A class representing a Quaternion.
 * Quaternions are used to represent rotations in 3D space and are elements of a non-commutative division algebra.
 * This implementation focuses on the algebraic properties. For spinlock applications, specific memory layout
 * or atomic operations might be needed depending on the exact use case.
 *
 * @details A quaternion q is represented as q = r + i*i + j*j + k*k, where r is the scalar part
 * and (i, j, k) are the vector part.
 */
class alignas(32) Quaternion {
public:
    double r; ///< Scalar part (w component)
    double i; ///< First vector component (x component)
    double j; ///< Second vector component (y component)
    double k; ///< Third vector component (z component)

    // Constructors
    /** @brief Default constructor, initializes to zero quaternion (0,0,0,0). */
    constexpr Quaternion() : r(0.0), i(0.0), j(0.0), k(0.0) {}

    /** @brief Constructor from four scalar components. */
    constexpr Quaternion(double r_val, double i_val, double j_val, double k_val)
        : r(r_val), i(i_val), j(j_val), k(k_val) {}

    /** @brief Constructor from a scalar part and a 3D vector part. */
    constexpr Quaternion(double scalar_part, const std::array<double, 3>& vector_part)
        : r(scalar_part), i(vector_part[0]), j(vector_part[1]), k(vector_part[2]) {}


    // Operators
    Quaternion& operator+=(const Quaternion& other);
    Quaternion& operator-=(const Quaternion& other);
    Quaternion& operator*=(double scalar);
    Quaternion& operator/=(double scalar);
    Quaternion& operator*=(const Quaternion& other); // Hamilton product

    // Friend operators for non-member versions
    friend Quaternion operator+(Quaternion lhs, const Quaternion& rhs);
    friend Quaternion operator-(Quaternion lhs, const Quaternion& rhs);
    friend Quaternion operator*(Quaternion lhs, double scalar);
    friend Quaternion operator*(double scalar, Quaternion rhs);
    friend Quaternion operator/(Quaternion lhs, double scalar);
    friend Quaternion operator*(Quaternion lhs, const Quaternion& rhs); // Hamilton product

    // Comparison operators
    bool operator==(const Quaternion& other) const;
    bool operator!=(const Quaternion& other) const;

    // Methods
    /** @brief Computes the conjugate of the quaternion. */
    constexpr Quaternion conjugate() const {
        return Quaternion(r, -i, -j, -k);
    }

    /** @brief Computes the squared norm (magnitude squared) of the quaternion.
     *  This is more efficient than norm() if only comparison of magnitudes is needed.
     */
    constexpr double norm_sq() const {
        return r * r + i * i + j * j + k * k;
    }

    /** @brief Computes the norm (magnitude) of the quaternion. */
    double norm() const;

    /** @brief Computes the inverse of the quaternion.
     *  Returns a zero quaternion if the original quaternion is zero (norm_sq is zero).
     */
    Quaternion inverse() const;

    /** @brief Normalizes the quaternion to unit length.
     *  If the quaternion is zero, it remains zero.
     */
    Quaternion& normalize();

    /** @brief Returns a normalized version of this quaternion. */
    Quaternion normalized() const;

    /** @brief Checks if the quaternion is a unit quaternion (norm is close to 1). */
    bool is_unit(double tolerance = 1e-9) const;

    // Static members
    /** @brief Returns the identity quaternion (1,0,0,0). */
    static constexpr Quaternion identity() { return Quaternion(1.0, 0.0, 0.0, 0.0); }
    /** @brief Returns the zero quaternion (0,0,0,0). */
    static constexpr Quaternion zero() { return Quaternion(0.0, 0.0, 0.0, 0.0); }

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Quaternion& q);
};

} // namespace Math
} // namespace Common

#endif // COMMON_MATH_QUATERNION_HPP

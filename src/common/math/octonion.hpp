#ifndef COMMON_MATH_OCTONION_HPP
#define COMMON_MATH_OCTONION_HPP

#include "quaternion.hpp" // May be used for Cayley-Dickson, or for inspiration
#include <array>
#include <cmath>    // For std::sqrt
#include <iosfwd>   // For ostream declaration

namespace Common {
namespace Math {

/**
 * @brief A class representing an Octonion.
 * Octonions are elements of a non-associative, non-commutative division algebra.
 * They extend quaternions via the Cayley-Dickson construction.
 *
 * @details An octonion o is represented as o = e0 + e1*i1 + e2*i2 + ... + e7*i7.
 * Multiplication is not associative: (a*b)*c is not necessarily equal to a*(b*c).
 * This implementation might use two Quaternions internally for some operations if beneficial,
 * or operate directly on an array of 8 components.
 * For capability/token representation, specific properties of octonions might be leveraged,
 * such as their relation to E8 lattice or specific subalgebras.
 */
class alignas(64) Octonion { // Align to 64 bytes for 8 doubles (AVX-512 potential)
public:
    std::array<double, 8> c; ///< Components e0, e1, e2, e3, e4, e5, e6, e7

    // Constructors
    /** @brief Default constructor, initializes to zero octonion. */
    constexpr Octonion() : c{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0} {}

    /** @brief Constructor from eight scalar components. */
    constexpr Octonion(double c0, double c1, double c2, double c3,
                       double c4, double c5, double c6, double c7)
        : c{c0, c1, c2, c3, c4, c5, c6, c7} {}

    /** @brief Constructor from an array of 8 doubles. */
    constexpr explicit Octonion(const std::array<double, 8>& components) : c(components) {}

    /** @brief Constructor from two Quaternions (a, b) representing a + b*e4. */
    Octonion(const Quaternion& a, const Quaternion& b);


    // Operators
    Octonion& operator+=(const Octonion& other);
    Octonion& operator-=(const Octonion& other);
    Octonion& operator*=(double scalar);
    Octonion& operator/=(double scalar);
    Octonion& operator*=(const Octonion& other); // Non-associative product

    // Friend operators for non-member versions
    friend Octonion operator+(Octonion lhs, const Octonion& rhs);
    friend Octonion operator-(Octonion lhs, const Octonion& rhs);
    friend Octonion operator*(Octonion lhs, double scalar);
    friend Octonion operator*(double scalar, Octonion rhs);
    friend Octonion operator/(Octonion lhs, double scalar);
    friend Octonion operator*(Octonion lhs, const Octonion& rhs);

    // Comparison operators
    bool operator==(const Octonion& other) const;
    bool operator!=(const Octonion& other) const;

    // Methods
    /** @brief Computes the conjugate of the octonion. */
    constexpr Octonion conjugate() const {
        return Octonion(c[0], -c[1], -c[2], -c[3], -c[4], -c[5], -c[6], -c[7]);
    }

    /** @brief Computes the squared norm (magnitude squared). */
    constexpr double norm_sq() const {
        double sum_sq = 0.0;
        for (double val : c) {
            sum_sq += val * val;
        }
        return sum_sq;
    }

    /** @brief Computes the norm (magnitude). */
    double norm() const;

    /** @brief Computes the inverse of the octonion.
     *  Returns a zero octonion if the original is zero.
     */
    Octonion inverse() const;

    /** @brief Normalizes the octonion to unit length.
     *  If the octonion is zero, it remains zero.
     */
    Octonion& normalize();

    /** @brief Returns a normalized version of this octonion. */
    Octonion normalized() const;

    /** @brief Checks if the octonion is a unit octonion (norm is close to 1). */
    bool is_unit(double tolerance = 1e-9) const;

    // Static members
    /** @brief Returns the identity octonion (1,0,...,0). */
    static constexpr Octonion identity() {
        return Octonion(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }
    /** @brief Returns the zero octonion (0,...,0). */
    static constexpr Octonion zero() {
        return Octonion(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }

    // Access to Quaternion parts for Cayley-Dickson based operations if needed
    Quaternion get_q1() const; // First quaternion part (e0-e3)
    Quaternion get_q2() const; // Second quaternion part (e4-e7)

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Octonion& o);
};

} // namespace Math
} // namespace Common

#endif // COMMON_MATH_OCTONION_HPP

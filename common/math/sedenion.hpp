#ifndef COMMON_MATH_SEDENION_HPP
#define COMMON_MATH_SEDENION_HPP

#include "octonion.hpp" // For Cayley-Dickson construction from Octonions
#include <array>
#include <cmath>    // For std::sqrt
#include <iosfwd>   // For ostream declaration

namespace Common {
namespace Math {

/**
 * @brief A class representing a Sedenion.
 * Sedenions are 16-dimensional hypercomplex numbers obtained by applying the
 * Cayley-Dickson construction to octonions.
 *
 * @details Sedenions are non-associative and non-commutative. Unlike quaternions and
 * octonions, sedenions are not a division algebra; they possess zero divisors
 * (i.e., two non-zero sedenions can multiply to yield zero).
 * This property is significant. For applications in physics or quantum information,
 * specific subalgebras or nilpotent/idempotent elements might be of interest.
 * For example, certain types of nilpotent sedenions (s*s = 0 where s != 0)
 * could be relevant for representing pre-spacetime structures or specific quantum states.
 * The management of zero divisors means that `inverse()` is not generally defined for all non-zero sedenions.
 * This implementation will provide arithmetic operations, but users must be cautious about division and inverses.
 *
 * A sedenion s is represented as s = e0 + e1*i1 + ... + e15*i15.
 */
class alignas(128) Sedenion { // Align to 128 bytes for 16 doubles
public:
    std::array<double, 16> c; ///< Components e0, ..., e15

    // Constructors
    /** @brief Default constructor, initializes to zero sedenion. */
    constexpr Sedenion() : c{} { // Value-initialize to all zeros
        for(size_t i=0; i<16; ++i) c[i] = 0.0;
    }


    /** @brief Constructor from sixteen scalar components. */
    constexpr Sedenion(
        double c0, double c1, double c2, double c3, double c4, double c5, double c6, double c7,
        double c8, double c9, double c10, double c11, double c12, double c13, double c14, double c15)
        : c{c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15} {}

    /** @brief Constructor from an array of 16 doubles. */
    constexpr explicit Sedenion(const std::array<double, 16>& components) : c(components) {}

    /** @brief Constructor from two Octonions (a, b) representing S = a + b*e8. */
    Sedenion(const Octonion& a, const Octonion& b);

    // Operators
    Sedenion& operator+=(const Sedenion& other);
    Sedenion& operator-=(const Sedenion& other);
    Sedenion& operator*=(double scalar);
    Sedenion& operator/=(double scalar); // Division by scalar
    Sedenion& operator*=(const Sedenion& other); // Non-associative product with zero divisors

    // Friend operators
    friend Sedenion operator+(Sedenion lhs, const Sedenion& rhs);
    friend Sedenion operator-(Sedenion lhs, const Sedenion& rhs);
    friend Sedenion operator*(Sedenion lhs, double scalar);
    friend Sedenion operator*(double scalar, Sedenion rhs);
    friend Sedenion operator/(Sedenion lhs, double scalar);
    friend Sedenion operator*(Sedenion lhs, const Sedenion& rhs);

    // Comparison operators
    bool operator==(const Sedenion& other) const;
    bool operator!=(const Sedenion& other) const;

    // Methods
    /** @brief Computes the conjugate of the sedenion. */
    constexpr Sedenion conjugate() const {
        Sedenion res;
        res.c[0] = c[0];
        for (size_t i = 1; i < 16; ++i) {
            res.c[i] = -c[i];
        }
        return res;
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

    /**
     * @brief Computes the inverse of the sedenion if it exists.
     * Sedenions are not a division algebra, so non-zero sedenions may not have an inverse
     * if they are zero divisors. This method returns s.conjugate() / s.norm_sq().
     * If norm_sq() is zero (which can happen for non-zero sedenions if they are zero divisors
     * of a specific form, e.g. nilpotent s^2=0 implies s*s_conj=0 if s_conj is related to s),
     * this will result in division by zero or NaN/inf values.
     * A more robust inverse would check if norm_sq is non-zero.
     * @return The inverse if norm_sq is non-zero, otherwise behavior is undefined or results in NaN/inf.
     */
    Sedenion inverse() const;

    /** @brief Normalizes the sedenion to unit length if possible.
     *  If the sedenion has zero norm (e.g. it's a zero divisor like (e1+e10)), it remains zero or results in NaN.
     */
    Sedenion& normalize();

    /** @brief Returns a normalized version of this sedenion if possible. */
    Sedenion normalized() const;

    /** @brief Checks if the sedenion is a unit sedenion (norm is close to 1). */
    bool is_unit(double tolerance = 1e-9) const;

    // Zero divisor related utilities (conceptual for now)
    /** @brief Checks if the sedenion is a zero divisor. (Complex to implement generally)
     *  A simple check could be if norm_sq() is zero for a non-zero Sedenion.
     *  e.g. (e1 + e10) has e1^2 = -1, e10^2 = -1, e1*e10 = -e11, e10*e1 = e11.
     *  (e1+e10)*(e1-e10) = e1*e1 - e1*e10 + e10*e1 - e10*e10 = -1 - (-e11) + e11 - (-1) = 2*e11 != 0
     *  (e3+e10)*(e5−e12) = 0 is a known zero divisor pair.
     */
    bool is_zero_divisor_candidate() const; // Placeholder

    // Static members
    static constexpr Sedenion identity() {
        std::array<double, 16> s_c{}; s_c[0] = 1.0; return Sedenion(s_c);
    }
    static constexpr Sedenion zero() {
        std::array<double, 16> s_c{}; return Sedenion(s_c);
    }

    // Access to Octonion parts
    Octonion get_o1() const; // First octonion part (e0-e7)
    Octonion get_o2() const; // Second octonion part (e8-e15)

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Sedenion& s);
};

} // namespace Math
} // namespace Common

#endif // COMMON_MATH_SEDENION_HPP

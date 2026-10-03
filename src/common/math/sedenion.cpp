#include "sedenion.hpp"
#include "octonion.hpp" // For Cayley-Dickson
#include <cmath>      // For std::sqrt, std::fabs
#include <stdexcept>  // For std::runtime_error
#include <ostream>
#include <iomanip>    // For std::fixed, std::setprecision
#include <numeric>    // For std::inner_product (or manual loop for norm_sq)

namespace Common {
namespace Math {

// Constructor from two Octonions
Sedenion::Sedenion(const Octonion& o1, const Octonion& o2) {
    for (size_t i = 0; i < 8; ++i) {
        c[i] = o1.c[i];
        c[i + 8] = o2.c[i];
    }
}

// Helper to get Octonion parts
Octonion Sedenion::get_o1() const {
    std::array<double, 8> o1_comps;
    for (size_t i = 0; i < 8; ++i) o1_comps[i] = c[i];
    return Octonion(o1_comps);
}

Octonion Sedenion::get_o2() const {
    std::array<double, 8> o2_comps;
    for (size_t i = 0; i < 8; ++i) o2_comps[i] = c[i+8];
    return Octonion(o2_comps);
}

// Operators
Sedenion& Sedenion::operator+=(const Sedenion& other) {
    for (size_t i = 0; i < 16; ++i) {
        c[i] += other.c[i];
    }
    return *this;
}

Sedenion& Sedenion::operator-=(const Sedenion& other) {
    for (size_t i = 0; i < 16; ++i) {
        c[i] -= other.c[i];
    }
    return *this;
}

Sedenion& Sedenion::operator*=(double scalar) {
    for (size_t i = 0; i < 16; ++i) {
        c[i] *= scalar;
    }
    return *this;
}

Sedenion& Sedenion::operator/=(double scalar) {
    if (std::fabs(scalar) < 1e-12) { // Use a small epsilon for floating point comparisons
        throw std::runtime_error("Sedenion division by zero scalar.");
    }
    for (size_t i = 0; i < 16; ++i) {
        c[i] /= scalar;
    }
    return *this;
}

Sedenion& Sedenion::operator*=(const Sedenion& other) {
    // Cayley-Dickson construction: (a, b) * (c, d) = (ac - d*conj(b), d*a + conj(c)*b)
    // where a,b are Octonions from *this, c,d are Octonions from other.
    Octonion a = this->get_o1();
    Octonion b = this->get_o2();
    Octonion c_other = other.get_o1();
    Octonion d_other = other.get_o2();

    Octonion res_o1 = (a * c_other) - (d_other * b.conjugate());
    Octonion res_o2 = (d_other * a) + (c_other.conjugate() * b);

    for (size_t i = 0; i < 8; ++i) {
        this->c[i] = res_o1.c[i];
        this->c[i + 8] = res_o2.c[i];
    }
    return *this;
}

// Friend operators
Sedenion operator+(Sedenion lhs, const Sedenion& rhs) {
    lhs += rhs;
    return lhs;
}

Sedenion operator-(Sedenion lhs, const Sedenion& rhs) {
    lhs -= rhs;
    return lhs;
}

Sedenion operator*(Sedenion lhs, double scalar) {
    lhs *= scalar;
    return lhs;
}

Sedenion operator*(double scalar, Sedenion rhs) {
    rhs *= scalar;
    return rhs;
}

Sedenion operator/(Sedenion lhs, double scalar) {
    lhs /= scalar;
    return lhs;
}

Sedenion operator*(Sedenion lhs, const Sedenion& rhs) {
    lhs *= rhs;
    return lhs;
}

// Comparison operators
bool Sedenion::operator==(const Sedenion& other) const {
    double epsilon = 1e-9; // Adjust tolerance as needed
    for (size_t i = 0; i < 16; ++i) {
        if (std::fabs(c[i] - other.c[i]) >= epsilon) {
            return false;
        }
    }
    return true;
}

bool Sedenion::operator!=(const Sedenion& other) const {
    return !(*this == other);
}

// Methods
double Sedenion::norm() const {
    return std::sqrt(norm_sq());
}

Sedenion Sedenion::inverse() const {
    double n_sq = norm_sq();
    // For Sedenions, norm_sq can be zero for non-zero Sedenions (zero divisors).
    // Example: (e1 + e10) is non-zero. (e1+e10)*(e1-e10) = 2*e11.
    // However, (e1+e10)*(e1+e10).conjugate() = (e1+e10)*(e1_conj+e10_conj) = (e1+e10)*(-e1-e10)
    // = -(e1+e10)*(e1+e10) = -(e1^2 + e1e10 + e10e1 + e10^2) = -(-1 -e11 +e11 -1) = -(-2) = 2.
    // A Sedenion s is a zero divisor if there exists a non-zero Sedenion t such that st = 0.
    // If s is a zero divisor, it does not have a unique inverse.
    // The formula s_inv = s_conj / norm_sq(s) only works if norm_sq(s) != 0.
    if (std::fabs(n_sq) < 1e-12) { // If norm_sq is zero
        // This Sedenion is a zero divisor or the zero Sedenion itself.
        // Returning a zero Sedenion, or NaN/inf by direct division, or throwing.
        // Throwing might be more appropriate for "inverse undefined".
        // For now, return zero to avoid NaN propagation by default.
        // Consider logging a warning or specific error handling for production code.
        // console_write_string("Warning: Attempting to invert a Sedenion with zero norm.\n");
        return Sedenion::zero();
    }
    return conjugate() / n_sq;
}

Sedenion& Sedenion::normalize() {
    double n = norm();
    if (std::fabs(n) < 1e-12) {
        // Cannot normalize a sedenion with zero norm. It remains zero or becomes NaN/inf.
        for(size_t i=0; i<16; ++i) c[i] = 0.0;
        return *this;
    }
    *this /= n;
    return *this;
}

Sedenion Sedenion::normalized() const {
    Sedenion s = *this;
    s.normalize();
    return s;
}

bool Sedenion::is_unit(double tolerance) const {
    return std::fabs(norm_sq() - 1.0) < tolerance * tolerance;
}

bool Sedenion::is_zero_divisor_candidate() const {
    if (this->operator==(Sedenion::zero())) return false; // Zero is not a zero divisor by typical def.
    return std::fabs(norm_sq()) < 1e-12; // Non-zero Sedenions with zero norm are zero divisors.
}


// Stream output
std::ostream& operator<<(std::ostream& os, const Sedenion& s) {
    std::ios_base::fmtflags flags = os.flags();
    os << std::fixed << std::setprecision(4);
    os << "(";
    for (size_t i = 0; i < 16; ++i) {
        os << s.c[i] << (i == 0 ? "" : ("e" + std::to_string(i)));
        if (i < 15) os << ", ";
    }
    os << ")";
    os.flags(flags);
    return os;
}

} // namespace Math
} // namespace Common

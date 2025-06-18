#include "octonion.hpp"
#include "quaternion.hpp" // For Cayley-Dickson construction
#include <cmath>        // For std::sqrt, std::fabs
#include <stdexcept>    // For std::runtime_error
#include <ostream>
#include <iomanip>      // For std::fixed, std::setprecision
#include <numeric>      // For std::inner_product (potentially, or manual loop for norm_sq)

namespace Common {
namespace Math {

// Constructor from two Quaternions
Octonion::Octonion(const Quaternion& q1, const Quaternion& q2) {
    c[0] = q1.r; c[1] = q1.i; c[2] = q1.j; c[3] = q1.k;
    c[4] = q2.r; c[5] = q2.i; c[6] = q2.j; c[7] = q2.k;
}

// Helper to get Quaternion parts
Quaternion Octonion::get_q1() const {
    return Quaternion(c[0], c[1], c[2], c[3]);
}

Quaternion Octonion::get_q2() const {
    return Quaternion(c[4], c[5], c[6], c[7]);
}


// Operators
Octonion& Octonion::operator+=(const Octonion& other) {
    for (size_t i = 0; i < 8; ++i) {
        c[i] += other.c[i];
    }
    return *this;
}

Octonion& Octonion::operator-=(const Octonion& other) {
    for (size_t i = 0; i < 8; ++i) {
        c[i] -= other.c[i];
    }
    return *this;
}

Octonion& Octonion::operator*=(double scalar) {
    for (size_t i = 0; i < 8; ++i) {
        c[i] *= scalar;
    }
    return *this;
}

Octonion& Octonion::operator/=(double scalar) {
    if (std::fabs(scalar) < 1e-12) {
        throw std::runtime_error("Octonion division by zero scalar.");
    }
    for (size_t i = 0; i < 8; ++i) {
        c[i] /= scalar;
    }
    return *this;
}

Octonion& Octonion::operator*=(const Octonion& other) {
    // Cayley-Dickson construction: (a, b) * (c, d) = (ac - d*conj(b), d*a + conj(c)*b)
    // where a,b are from *this, c,d are from other.
    // a = this.get_q1(), b = this.get_q2()
    // c = other.get_q1(), d = other.get_q2()
    Quaternion a = this->get_q1();
    Quaternion b = this->get_q2();
    Quaternion c_other = other.get_q1();
    Quaternion d_other = other.get_q2();

    Quaternion res_q1 = (a * c_other) - (d_other * b.conjugate());
    Quaternion res_q2 = (d_other * a) + (c_other.conjugate() * b);

    this->c[0] = res_q1.r; this->c[1] = res_q1.i; this->c[2] = res_q1.j; this->c[3] = res_q1.k;
    this->c[4] = res_q2.r; this->c[5] = res_q2.i; this->c[6] = res_q2.j; this->c[7] = res_q2.k;

    return *this;
}

// Friend operators
Octonion operator+(Octonion lhs, const Octonion& rhs) {
    lhs += rhs;
    return lhs;
}

Octonion operator-(Octonion lhs, const Octonion& rhs) {
    lhs -= rhs;
    return lhs;
}

Octonion operator*(Octonion lhs, double scalar) {
    lhs *= scalar;
    return lhs;
}

Octonion operator*(double scalar, Octonion rhs) {
    rhs *= scalar;
    return rhs;
}

Octonion operator/(Octonion lhs, double scalar) {
    lhs /= scalar;
    return lhs;
}

Octonion operator*(Octonion lhs, const Octonion& rhs) {
    lhs *= rhs; // Non-associative product
    return lhs;
}

// Comparison operators
bool Octonion::operator==(const Octonion& other) const {
    double epsilon = 1e-9; // Adjust tolerance as needed
    for (size_t i = 0; i < 8; ++i) {
        if (std::fabs(c[i] - other.c[i]) >= epsilon) {
            return false;
        }
    }
    return true;
}

bool Octonion::operator!=(const Octonion& other) const {
    return !(*this == other);
}

// Methods
double Octonion::norm() const {
    return std::sqrt(norm_sq());
}

Octonion Octonion::inverse() const {
    double n_sq = norm_sq();
    if (std::fabs(n_sq) < 1e-12) {
        return Octonion::zero();
    }
    return conjugate() / n_sq;
}

Octonion& Octonion::normalize() {
    double n = norm();
    if (std::fabs(n) < 1e-12) {
        for(size_t i=0; i<8; ++i) c[i] = 0.0; // Ensure it's exactly zero if norm is too small
        return *this;
    }
    *this /= n;
    return *this;
}

Octonion Octonion::normalized() const {
    Octonion o = *this;
    o.normalize();
    return o;
}

bool Octonion::is_unit(double tolerance) const {
    return std::fabs(norm_sq() - 1.0) < tolerance * tolerance;
}

// Stream output
std::ostream& operator<<(std::ostream& os, const Octonion& o) {
    std::ios_base::fmtflags flags = os.flags();
    os << std::fixed << std::setprecision(4);
    os << "(";
    for (size_t i = 0; i < 8; ++i) {
        os << o.c[i] << (i == 0 ? "" : ("e" + std::to_string(i)));
        if (i < 7) os << ", ";
    }
    os << ")";
    os.flags(flags);
    return os;
}

} // namespace Math
} // namespace Common

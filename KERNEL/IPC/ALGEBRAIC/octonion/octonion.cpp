#include "octonion.hpp" // Will resolve to KERNEL/IPC/ALGEBRAIC/octonion/octonion.hpp
// The header "octonion.hpp" should include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp"
#include <cmath>        // For std::sqrt, std::fabs
#include <stdexcept>    // For std::runtime_error (though we are removing its use)
#include <ostream>
#include <iomanip>      // For std::fixed, std::setprecision
#include <limits>       // For std::numeric_limits (used for OCTONION_EPSILON in header)

namespace XINIM { // Changed namespace
namespace Algebraic {

// Constructor from two Quaternions
Octonion::Octonion(const XINIM::Algebraic::Quaternion& q1, const XINIM::Algebraic::Quaternion& q2) noexcept {
    // Assuming q1.w, q1.x, q1.y, q1.z and similar for q2 are the correct component names
    // from the XINIM::Algebraic::Quaternion class.
    c[0] = q1.w; c[1] = q1.x; c[2] = q1.y; c[3] = q1.z; // q1 maps to e0-e3
    c[4] = q2.w; c[5] = q2.x; c[6] = q2.y; c[7] = q2.z; // q2 maps to e4-e7
}

// Helper to get Quaternion parts
XINIM::Algebraic::Quaternion Octonion::get_q1() const noexcept {
    return XINIM::Algebraic::Quaternion(c[0], c[1], c[2], c[3]);
}

XINIM::Algebraic::Quaternion Octonion::get_q2() const noexcept {
    return XINIM::Algebraic::Quaternion(c[4], c[5], c[6], c[7]);
}

// Operators (member versions)
// +=, -=, *= (scalar) are constexpr and defined in header.

Octonion& Octonion::operator/=(double scalar) noexcept {
    if (std::fabs(scalar) < OCTONION_EPSILON) {
        // Division by a number very close to zero. Set to zero octonion.
        for(size_t i=0; i<8; ++i) c[i] = 0.0;
        return *this;
    }
    for (size_t i = 0; i < 8; ++i) {
        c[i] /= scalar;
    }
    return *this;
}

Octonion& Octonion::operator*=(const Octonion& other) noexcept {
    // Cayley-Dickson construction: (a, b) * (c, d) = (ac - d*conj(b), d*a + conj(c)*b)
    // where a,b are from *this, c,d are from other.
    XINIM::Algebraic::Quaternion a = this->get_q1();
    XINIM::Algebraic::Quaternion b = this->get_q2();
    XINIM::Algebraic::Quaternion c_other = other.get_q1();
    XINIM::Algebraic::Quaternion d_other = other.get_q2();

    XINIM::Algebraic::Quaternion res_q1 = (a * c_other) - (d_other * b.conjugate());
    XINIM::Algebraic::Quaternion res_q2 = (d_other * a) + (c_other.conjugate() * b);

    this->c[0] = res_q1.w; this->c[1] = res_q1.x; this->c[2] = res_q1.y; this->c[3] = res_q1.z;
    this->c[4] = res_q2.w; this->c[5] = res_q2.x; this->c[6] = res_q2.y; this->c[7] = res_q2.z;
    return *this;
}

// Friend operators are defined in header.
// Comparison operators (==, !=) are constexpr and defined in header.

// Methods
double Octonion::norm() const noexcept {
    return std::sqrt(norm_sq()); // norm_sq() is constexpr noexcept
}

Octonion Octonion::inverse() const noexcept {
    double n_sq = norm_sq(); // norm_sq() is constexpr noexcept
    if (std::fabs(n_sq) < OCTONION_EPSILON) { // Use OCTONION_EPSILON
        return Octonion::zero(); // zero() is constexpr noexcept
    }
    // conjugate() is constexpr noexcept, operator/(double) for Octonion is noexcept
    return conjugate() / n_sq;
}

Octonion& Octonion::normalize() noexcept {
    double n = norm(); // norm() is noexcept
    if (std::fabs(n) < OCTONION_EPSILON) { // Use OCTONION_EPSILON
        for(size_t i=0; i<8; ++i) c[i] = 0.0; // Ensure it's exactly zero
        return *this;
    }
    return *this /= n; // operator/= is noexcept
}

Octonion Octonion::normalized() const noexcept {
    Octonion o = *this;
    o.normalize(); // normalize() is noexcept
    return o;
}

bool Octonion::is_unit(double tolerance) const noexcept {
    // Compare squared norm to 1 to avoid sqrt, and use squared tolerance
    return std::fabs(norm_sq() - 1.0) < tolerance * tolerance;
}

bool Octonion::is_zero(double tolerance) const noexcept {
    for (size_t i = 0; i < 8; ++i) {
        if (std::fabs(c[i]) >= tolerance) return false;
    }
    return true;
}

// Stream output
std::ostream& operator<<(std::ostream& os, const Octonion& o) {
    std::ios_base::fmtflags flags = os.flags(); // Save old flags
    os << std::fixed << std::setprecision(4); // Use fixed point with precision
    os << "o(";
    for (size_t i = 0; i < 8; ++i) {
        os << o.c[i] << (i == 0 ? "" : ("e" + std::to_string(i)));
        if (i < 7) os << ", ";
    }
    os << ")";
    os.flags(flags); // Restore old flags
    return os;
}

} // namespace Algebraic
} // namespace XINIM

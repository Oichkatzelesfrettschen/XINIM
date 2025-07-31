#include "quaternion.hpp" // Should pick up the KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp
#include <cmath>          // For std::sqrt, std::fabs
#include <stdexcept>      // For std::runtime_error (though we aim to remove its use here)
#include <ostream>
#include <iomanip>        // For std::fixed, std::setprecision
#include <limits>         // For std::numeric_limits

namespace XINIM { // Changed namespace
namespace Algebraic {

// Implementation of Quaternion methods

Quaternion& Quaternion::operator/=(double scalar) noexcept { // Changed to noexcept
    if (std::fabs(scalar) < QUATERNION_EPSILON) {
        // Division by a number very close to zero. Set to zero quaternion.
        *this = Quaternion::zero();
        return *this;
    }
    w /= scalar;
    x /= scalar;
    y /= scalar;
    z /= scalar;
    return *this;
}

Quaternion& Quaternion::operator*=(const Quaternion& rhs) noexcept {
    double w_new = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
    double x_new = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
    double y_new = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
    double z_new = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
    w = w_new;
    x = x_new;
    y = y_new;
    z = z_new;
    return *this;
}

// Note: Comparison operators (==, !=) are constexpr and defined in header.
// Note: Arithmetic operators (+, -, *, / non-member friends) are defined in header using members.

double Quaternion::norm() const noexcept {
    return std::sqrt(norm_sq());
}

Quaternion Quaternion::inverse() const noexcept {
    double n_sq = norm_sq();
    if (std::fabs(n_sq) < QUATERNION_EPSILON) {
        return Quaternion::zero(); // Return zero quaternion if norm is close to zero
    }
    // conjugate() is constexpr noexcept, operator/(double) for Quaternion is noexcept
    return conjugate() / n_sq;
}

Quaternion& Quaternion::normalize() noexcept {
    double n = norm(); // norm() is noexcept
    if (std::fabs(n) < QUATERNION_EPSILON) {
        // Cannot normalize a zero-norm quaternion. It remains zero.
        *this = Quaternion::zero();
        return *this;
    }
    return *this /= n; // operator/= is noexcept
}

Quaternion Quaternion::normalized() const noexcept {
    Quaternion q = *this;
    q.normalize(); // normalize() is noexcept
    return q;
}

bool Quaternion::is_unit(double tolerance) const noexcept {
    // Compare squared norm to 1 to avoid sqrt, and use squared tolerance
    return std::fabs(norm_sq() - 1.0) < tolerance * tolerance;
}

bool Quaternion::is_zero(double tolerance) const noexcept {
    return (std::fabs(w) < tolerance &&
            std::fabs(x) < tolerance &&
            std::fabs(y) < tolerance &&
            std::fabs(z) < tolerance);
}

// Stream output
std::ostream& operator<<(std::ostream& os, const Quaternion& q) {
    std::ios_base::fmtflags flags = os.flags(); // Save old flags
    os << std::fixed << std::setprecision(4); // Use fixed point with precision
    // Output w, x, y, z for consistency with component names
    os << "q(w:" << q.w << ", x:" << q.x << ", y:" << q.y << ", z:" << q.z << ")";
    os.flags(flags); // Restore old flags
    return os;
}

} // namespace Algebraic
} // namespace XINIM

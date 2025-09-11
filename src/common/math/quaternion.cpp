#include "quaternion.hpp"
#include <cmath> // For std::sqrt, std::fabs
#include <stdexcept> // For std::runtime_error (in division by zero scalar)
#include <ostream>
#include <iomanip> // For std::fixed, std::setprecision

namespace Common {
namespace Math {

// Operators
Quaternion& Quaternion::operator+=(const Quaternion& other) {
    r += other.r;
    i += other.i;
    j += other.j;
    k += other.k;
    return *this;
}

Quaternion& Quaternion::operator-=(const Quaternion& other) {
    r -= other.r;
    i -= other.i;
    j -= other.j;
    k -= other.k;
    return *this;
}

Quaternion& Quaternion::operator*=(double scalar) {
    r *= scalar;
    i *= scalar;
    j *= scalar;
    k *= scalar;
    return *this;
}

Quaternion& Quaternion::operator/=(double scalar) {
    if (std::fabs(scalar) < 1e-12) { // Consider a robust epsilon comparison
        // Or set to NaN/inf, or handle as per specific application requirements
        // For now, clear to zero or throw. Let's throw for immediate feedback.
        throw std::runtime_error("Quaternion division by zero scalar.");
    }
    r /= scalar;
    i /= scalar;
    j /= scalar;
    k /= scalar;
    return *this;
}

Quaternion& Quaternion::operator*=(const Quaternion& other) {
    double r_new = r * other.r - i * other.i - j * other.j - k * other.k;
    double i_new = r * other.i + i * other.r + j * other.k - k * other.j;
    double j_new = r * other.j - i * other.k + j * other.r + k * other.i;
    double k_new = r * other.k + i * other.j - j * other.i + k * other.r;
    r = r_new;
    i = i_new;
    j = j_new;
    k = k_new;
    return *this;
}

// Friend operators
Quaternion operator+(Quaternion lhs, const Quaternion& rhs) {
    lhs += rhs;
    return lhs;
}

Quaternion operator-(Quaternion lhs, const Quaternion& rhs) {
    lhs -= rhs;
    return lhs;
}

Quaternion operator*(Quaternion lhs, double scalar) {
    lhs *= scalar;
    return lhs;
}

Quaternion operator*(double scalar, Quaternion rhs) {
    rhs *= scalar; // Utilize the member operator*=
    return rhs;
}

Quaternion operator/(Quaternion lhs, double scalar) {
    lhs /= scalar;
    return lhs;
}

Quaternion operator*(Quaternion lhs, const Quaternion& rhs) {
    lhs *= rhs; // Hamilton product
    return lhs;
}

// Comparison operators
bool Quaternion::operator==(const Quaternion& other) const {
    // Using a small epsilon for floating point comparisons
    double epsilon = 1e-9; // Adjust tolerance as needed
    return (std::fabs(r - other.r) < epsilon &&
            std::fabs(i - other.i) < epsilon &&
            std::fabs(j - other.j) < epsilon &&
            std::fabs(k - other.k) < epsilon);
}

bool Quaternion::operator!=(const Quaternion& other) const {
    return !(*this == other);
}

// Methods
double Quaternion::norm() const {
    return std::sqrt(norm_sq());
}

Quaternion Quaternion::inverse() const {
    double n_sq = norm_sq();
    if (std::fabs(n_sq) < 1e-12) { // Check if norm_sq is close to zero
        // Return zero quaternion or handle error (e.g. throw)
        // Depending on application, a "pseudo-inverse" might not be meaningful here.
        return Quaternion(0.0, 0.0, 0.0, 0.0);
    }
    return conjugate() / n_sq;
}

Quaternion& Quaternion::normalize() {
    double n = norm();
    if (std::fabs(n) < 1e-12) {
        // Cannot normalize a zero quaternion. It remains zero.
        // Or could throw an error depending on requirements.
        r = 0.0; i = 0.0; j = 0.0; k = 0.0;
        return *this;
    }
    *this /= n;
    return *this;
}

Quaternion Quaternion::normalized() const {
    Quaternion q = *this;
    q.normalize();
    return q;
}

bool Quaternion::is_unit(double tolerance) const {
    return std::fabs(norm_sq() - 1.0) < tolerance * tolerance; // Compare squared norm to 1
}

// Stream output
std::ostream& operator<<(std::ostream& os, const Quaternion& q) {
    std::ios_base::fmtflags flags = os.flags(); // Save old flags
    os << std::fixed << std::setprecision(4); // Use fixed point with precision
    os << "(" << q.r << ", " << q.i << "i, " << q.j << "j, " << q.k << "k)";
    os.flags(flags); // Restore old flags
    return os;
}

} // namespace Math
} // namespace Common

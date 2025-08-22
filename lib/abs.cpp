/**
 * @file abs.cpp
 * @brief Implementation of a generic absolute value wrapper.
 */

#include <cmath>
#include <concepts>
#include <cstdlib>

namespace xinim {

/**
 * @brief Compute the absolute value of a numeric type.
 *
 * This template delegates to @c std::abs, leveraging the standard
 * library's overload set for integral and floating-point types.
 *
 * @tparam T Arithmetic type of the input value.
 * @param value The number to transform.
 * @return The non-negative magnitude of @p value.
 */
template <typename T>
    requires(std::integral<T> || std::floating_point<T>)
[[nodiscard]] constexpr T abs(T value) noexcept {
    return std::abs(value);
}

// Explicit instantiation for @c int to preserve previous interface.
template int abs<int>(int) noexcept;

} // namespace xinim

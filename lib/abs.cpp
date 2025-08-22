/**
 * @file abs.cpp
 * @brief Modern C++23 implementation of a generic absolute value wrapper.
 *
 * This file provides a generic, type-safe wrapper for computing the absolute
 * value of various numeric types. The implementation leverages the standard
 * library's `std::abs` function, ensuring correctness and efficiency while
 * presenting a clean and uniform interface.
 *
 * @ingroup utility
 */

#include <cmath>
#include <concepts>
#include <cstdlib>

namespace xinim {

/**
 * @brief Compute the absolute value of a numeric type.
 *
 * This template delegates to @c std::abs, leveraging the standard
 * library's overload set for integral and floating-point types. It is
 * declared `constexpr` to enable compile-time evaluation where possible,
 * aligning with modern C++ best practices.
 *
 * @tparam T An arithmetic type (integral or floating-point).
 * @param value The number to transform.
 * @return The non-negative magnitude of @p value.
 *
 * @sideeffects None.
 * @thread_safety Safe for concurrent use, as it operates on local data and
 * standard library functions designed for concurrency.
 * @compat abs(3)
 * @example
 * int i = -5;
 * int m = xinim::abs(i); // m is 5
 *
 * double d = -3.14;
 * double p = xinim::abs(d); // p is 3.14
 */
template <typename T>
    requires(std::integral<T> || std::floating_point<T>)
[[nodiscard]] constexpr T abs(T value) noexcept {
    return std::abs(value);
}

// Explicit instantiation for @c int to preserve previous interface.
template int abs<int>(int) noexcept;

} // namespace xinim
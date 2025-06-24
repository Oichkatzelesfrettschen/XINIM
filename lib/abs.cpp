/**
 * @file abs.cpp
 * @brief Implementation of the standard @c abs function.
 */

#include <cstdlib>

namespace xinim {

/**
 * @brief Compute the absolute value of an integer.
 *
 * @param i Input integer to evaluate.
 * @return The non-negative magnitude of @p i.
 */
[[nodiscard]] constexpr int abs(int i) noexcept { return (i < 0) ? -i : i; }

} // namespace xinim

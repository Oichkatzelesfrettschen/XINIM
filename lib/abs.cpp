/**
 * @file abs.cpp
 * @brief Implementation of the standard @c abs function.
 */

#include <stdlib.h>

/**
 * @brief Compute the absolute value of an integer.
 *
 * @param i Input integer.
 * @return Absolute value of @p i.
 */
[[nodiscard]] constexpr int abs(int i) noexcept { return (i < 0) ? -i : i; }

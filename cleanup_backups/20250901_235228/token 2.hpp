#pragma once
/**
 * @file token.hpp
 * @brief Capability token generation utilities.
 */

#include <cstdint>
#include <random>

/**
 * @brief Generate a cryptographically strong capability token.
 *
 * Uses a 64-bit Mersenne Twister seeded from @c std::random_device.
 *
 * @return Newly generated token value.
 */
[[nodiscard]] inline std::uint64_t generate_token() noexcept {
    static std::mt19937_64 engine{std::random_device{}()};
    static std::uniform_int_distribution<std::uint64_t> dist;
    return dist(engine);
}

/**
 * @file randombytes.hpp
 * @brief Modern C++23 random bytes generation
 *
 * Modernized random bytes generation using C++23 features with proper error handling.
 */

#pragma once

#include <cstdint>
#include <span>
#include <expected>
#include <system_error>
#include <memory>

namespace xinim::crypto::random {

// Error codes
enum class random_error {
    system_call_failed,
    invalid_output_buffer,
    insufficient_entropy
};

using error_code = std::error_code;
using random_result = std::expected<void, error_code>;

/**
 * @brief Fill buffer with cryptographically secure random bytes
 * @param out Output buffer span
 * @return Success or error code
 */
[[nodiscard]] random_result randombytes(std::span<std::uint8_t> out) noexcept;

/**
 * @brief Generate cryptographically secure random bytes with automatic buffer
 * @param length Number of bytes to generate
 * @return Random bytes buffer or error code
 */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, error_code>
randombytes(std::size_t length);

/**
 * @brief Fill a fixed-size array with random bytes
 * @tparam N Size of the array
 * @param out Output array
 * @return Success or error code
 */
template <std::size_t N>
[[nodiscard]] random_result randombytes(std::span<std::uint8_t, N> out) noexcept {
    return randombytes(std::span<std::uint8_t>{out});
}

/**
 * @brief Generate a fixed-size array of random bytes
 * @tparam N Number of bytes to generate
 * @return Random bytes array or error code
 */
template <std::size_t N>
[[nodiscard]] std::expected<std::array<std::uint8_t, N>, error_code> randombytes() {
    std::array<std::uint8_t, N> result{};
    if (auto err = randombytes(std::span<std::uint8_t, N>{result})) {
        return result;
    } else {
        return std::unexpected(err.error());
    }
}

} // namespace xinim::crypto::random
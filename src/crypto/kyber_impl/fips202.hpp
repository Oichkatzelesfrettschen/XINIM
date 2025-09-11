/**
 * @file fips202.hpp
 * @brief Modern C++23 FIPS 202 implementation (SHA-3, SHAKE128, SHAKE256)
 *
 * Modernized implementation for KYBER post-quantum cryptography using C++23 features.
 * This provides a clean, type-safe interface with proper error handling.
 */

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <expected>
#include <system_error>

namespace xinim::crypto::fips202 {

// Constants
inline constexpr std::size_t SHAKE128_RATE = 168;
inline constexpr std::size_t SHAKE256_RATE = 136;
inline constexpr std::size_t SHA3_256_OUTPUT_SIZE = 32;
inline constexpr std::size_t SHA3_512_OUTPUT_SIZE = 64;

// Error codes
enum class fips202_error {
    invalid_input_length,
    invalid_output_length,
    state_corruption
};

using error_code = std::error_code;
using fips202_result = std::expected<void, error_code>;

// Keccak state structure
struct keccak_state {
    std::array<std::uint64_t, 25> s{};
    std::size_t pos{0};

    constexpr keccak_state() noexcept = default;
};

/**
 * @brief SHAKE128 extendable output function
 * @param out Output buffer span
 * @param in Input buffer span
 * @return Success or error code
 */
[[nodiscard]] fips202_result shake128(std::span<std::uint8_t> out,
                                      std::span<const std::uint8_t> in) noexcept;

/**
 * @brief SHAKE256 extendable output function
 * @param out Output buffer span
 * @param in Input buffer span
 * @return Success or error code
 */
[[nodiscard]] fips202_result shake256(std::span<std::uint8_t> out,
                                      std::span<const std::uint8_t> in) noexcept;

/**
 * @brief SHAKE128 absorb operation
 * @param state SHAKE state
 * @param in Input buffer span
 * @return Success or error code
 */
[[nodiscard]] fips202_result shake128_absorb(keccak_state& state,
                                             std::span<const std::uint8_t> in) noexcept;

/**
 * @brief SHAKE128 squeeze blocks
 * @param out Output buffer span
 * @param state SHAKE state
 * @return Success or error code
 */
[[nodiscard]] fips202_result shake128_squeezeblocks(std::span<std::uint8_t> out,
                                                    keccak_state& state) noexcept;

/**
 * @brief SHAKE256 squeeze blocks
 * @param out Output buffer span
 * @param state SHAKE state
 * @return Success or error code
 */
[[nodiscard]] fips202_result shake256_squeezeblocks(std::span<std::uint8_t> out,
                                                    keccak_state& state) noexcept;

/**
 * @brief SHAKE256 squeeze operation
 * @param out Output buffer span
 * @param state SHAKE state
 * @return Success or error code
 */
[[nodiscard]] fips202_result shake256_squeeze(std::span<std::uint8_t> out,
                                              keccak_state& state) noexcept;

/**
 * @brief SHA3-256 hash function
 * @param out Output buffer (must be at least 32 bytes)
 * @param in Input buffer span
 * @return Success or error code
 */
[[nodiscard]] fips202_result sha3_256(std::span<std::uint8_t, SHA3_256_OUTPUT_SIZE> out,
                                      std::span<const std::uint8_t> in) noexcept;

/**
 * @brief SHA3-512 hash function
 * @param out Output buffer (must be at least 64 bytes)
 * @param in Input buffer span
 * @return Success or error code
 */
[[nodiscard]] fips202_result sha3_512(std::span<std::uint8_t, SHA3_512_OUTPUT_SIZE> out,
                                      std::span<const std::uint8_t> in) noexcept;

// Convenience functions with automatic buffer management

/**
 * @brief SHAKE128 with automatic output buffer
 * @param in Input buffer span
 * @param output_length Desired output length
 * @return Output buffer or error code
 */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, error_code>
shake128(std::span<const std::uint8_t> in, std::size_t output_length);

/**
 * @brief SHAKE256 with automatic output buffer
 * @param in Input buffer span
 * @param output_length Desired output length
 * @return Output buffer or error code
 */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, error_code>
shake256(std::span<const std::uint8_t> in, std::size_t output_length);

/**
 * @brief SHA3-256 with automatic output buffer
 * @param in Input buffer span
 * @return 32-byte hash or error code
 */
[[nodiscard]] std::expected<std::array<std::uint8_t, SHA3_256_OUTPUT_SIZE>, error_code>
sha3_256(std::span<const std::uint8_t> in);

/**
 * @brief SHA3-512 with automatic output buffer
 * @param in Input buffer span
 * @return 64-byte hash or error code
 */
[[nodiscard]] std::expected<std::array<std::uint8_t, SHA3_512_OUTPUT_SIZE>, error_code>
sha3_512(std::span<const std::uint8_t> in);

} // namespace xinim::crypto::fips202
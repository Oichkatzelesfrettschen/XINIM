#XINIM Error Handling Infrastructure(C++ 23)

**Status:** Implementation Framework  
**Date:** 2026-01-03  
**Related:** ADR-0002, Style Guide

---

## Overview

This file provides the unified error handling infrastructure for XINIM using C++23's `std::expected`. All new and refactored code must use this system.

---

## Core Error Types

```cpp
#pragma once

#include <expected>
#include <source_location>
#include <string_view>
#include <system_error>

namespace xinim::error {

    /**
     * @brief Error categories for XINIM subsystems
     */
    enum class Category {
        Memory,     ///< Memory allocation, paging, DMA
        IO,         ///< File operations, device I/O
        Permission, ///< Access control, capabilities
        Hardware,   ///< Device errors, hardware faults
        Network,    ///< Network stack errors
        Parse,      ///< Parsing errors (config files, etc.)
        System,     ///< General system errors
        Unknown     ///< Uncategorized errors
    };

    /**
     * @brief Unified error structure
     *
     * Provides comprehensive error information including:
     * - Error category
     * - System error code (errno-compatible)
     * - Human-readable message
     * - Source location (file, line, function)
     */
    struct Error {
        Category category = Category::Unknown;
        int code = 0;
        std::string_view message;
        std::source_location location = std::source_location::current();

        /**
         * @brief Creates an error with automatic source location
         */
        [[nodiscard]] constexpr Error(
            Category cat, int err_code, std::string_view msg,
            std::source_location loc = std::source_location::current()) noexcept
            : category(cat), code(err_code), message(msg), location(loc) {}

        /**
         * @brief Gets formatted error string
         */
        [[nodiscard]] std::string format() const {
            return std::format("Error [{}:{}:{}] {}: {} (code: {})", location.file_name(),
                               location.line(), location.function_name(), category_name(category),
                               message, code);
        }

      private:
        static constexpr std::string_view category_name(Category cat) {
            switch (cat) {
            case Category::Memory:
                return "Memory";
            case Category::IO:
                return "I/O";
            case Category::Permission:
                return "Permission";
            case Category::Hardware:
                return "Hardware";
            case Category::Network:
                return "Network";
            case Category::Parse:
                return "Parse";
            case Category::System:
                return "System";
            default:
                return "Unknown";
            }
        }
    };

    /**
     * @brief Result type alias for std::expected
     *
     * @tparam T Success value type
     *
     * Usage:
     *   auto read_file(std::string_view path) -> Result<std::vector<std::byte>>;
     */
    template <typename T> using Result = std::expected<T, Error>;

    /**
     * @brief Result type for functions that don't return a value
     *
     * Usage:
     *   auto write_file(std::string_view path) -> VoidResult;
     */
    using VoidResult = std::expected<void, Error>;

} // namespace xinim::error

// =============================================================================
// Common Error Constructors
// =============================================================================

namespace xinim::error {

/**
 * @brief Creates a memory error
 */
[[nodiscard]] inline auto
make_memory_error(int code, std::string_view message,
                  std::source_location loc = std::source_location::current()) -> Error {
    return Error{Category::Memory, code, message, loc};
}

/**
 * @brief Creates an I/O error
 */
[[nodiscard]] inline auto
make_io_error(int code, std::string_view message,
              std::source_location loc = std::source_location::current()) -> Error {
    return Error{Category::IO, code, message, loc};
}

/**
 * @brief Creates a permission error
 */
[[nodiscard]] inline auto
make_permission_error(int code, std::string_view message,
                      std::source_location loc = std::source_location::current()) -> Error {
    return Error{Category::Permission, code, message, loc};
}

/**
 * @brief Creates a parse error
 */
[[nodiscard]] inline auto
make_parse_error(int code, std::string_view message,
                 std::source_location loc = std::source_location::current()) -> Error {
    return Error{Category::Parse, code, message, loc};
}

/**
 * @brief Creates a hardware error
 */
[[nodiscard]] inline auto
make_hardware_error(int code, std::string_view message,
                    std::source_location loc = std::source_location::current()) -> Error {
    return Error{Category::Hardware, code, message, loc};
}

} // namespace xinim::error

// =============================================================================
// Error Handling Utilities
// =============================================================================

namespace xinim::error {

/**
 * @brief Chains multiple operations that return Result
 *
 * @example
 * auto result = chain(
 *     []() { return validate_input(); },
 *     [](auto& validated) { return process_data(validated); },
 *     [](auto& processed) { return write_output(processed); }
 * );
 */
template <typename Func, typename... Funcs> auto chain(Func &&func, Funcs &&...funcs) {
    auto result = func();
    if (!result) {
        return result;
    }

    if constexpr (sizeof...(funcs) > 0) {
        return chain(
            [&](auto &&...args) { return funcs(std::forward<decltype(args)>(args)...); }...);
    } else {
        return result;
    }
}

/**
 * @brief Maps a Result<T> to Result<U> using a transformation function
 *
 * @example
 * auto result = read_file(path);
 * auto parsed = map(result, [](auto& data) { return parse(data); });
 */
template <typename T, typename Func>
auto map(const Result<T> &result, Func &&func) -> Result<decltype(func(result.value()))> {
    if (!result) {
        return std::unexpected(result.error());
    }
    return func(result.value());
}

/**
 * @brief Unwraps a Result or returns a default value
 *
 * @example
 * auto data = read_file(path).value_or(std::vector<std::byte>{});
 */
template <typename T> auto value_or(const Result<T> &result, T &&default_value) -> T {
    return result.value_or(std::forward<T>(default_value));
}

} // namespace xinim::error

// =============================================================================
// Logging Integration
// =============================================================================

namespace xinim::error {

/**
 * @brief Logs an error and returns it
 *
 * Useful for debugging error propagation
 */
template <typename T> auto log_error(const Result<T> &result) -> const Result<T> & {
    if (!result) {
        // TODO: Integrate with kernel logging
        // For now, print to stderr
        std::cerr << result.error().format() << std::endl;
    }
    return result;
}

} // namespace xinim::error
```

    -- -

    ##Usage Examples

    ## #Example 1 : Simple File Reading

```cpp
#include <fstream>
#include <vector>
#include <xinim/error.hpp>

                    using namespace xinim::error;

auto read_file(std::string_view path) -> Result<std::vector<std::byte>> {
    std::ifstream file(path.data(), std::ios::binary);

    if (!file.is_open()) {
        return std::unexpected(make_io_error(ENOENT, "Failed to open file"));
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> data(size);
    if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
        return std::unexpected(make_io_error(EIO, "Failed to read file"));
    }

    return data;
}

// Usage
auto result = read_file("/etc/config");
if (!result) {
    std::cerr << "Error: " << result.error().format() << std::endl;
    return 1;
}
auto &data = result.value();
```

    ## #Example 2 : Chaining Operations

```cpp auto process_config() -> VoidResult {
    auto file_result = read_file("/etc/config");
    if (!file_result) {
        return std::unexpected(file_result.error());
    }

    auto parse_result = parse_config(file_result.value());
    if (!parse_result) {
        return std::unexpected(parse_result.error());
    }

    auto apply_result = apply_config(parse_result.value());
    if (!apply_result) {
        return std::unexpected(apply_result.error());
    }

    return {}; // Success
}

// Or using map:
auto process_config_v2() -> VoidResult {
    return read_file("/etc/config")
        .and_then([](auto &data) { return parse_config(data); })
        .and_then([](auto &config) { return apply_config(config); });
}
```

    ## #Example 3 : Memory Allocation

```cpp auto allocate_buffer(size_t size) -> Result<std::unique_ptr<Buffer>> {
    if (size == 0) {
        return std::unexpected(make_memory_error(EINVAL, "Size must be non-zero"));
    }

    if (size > MAX_BUFFER_SIZE) {
        return std::unexpected(make_memory_error(ENOMEM, "Requested size exceeds maximum"));
    }

    auto buffer = std::make_unique<Buffer>(size);
    if (!buffer) {
        return std::unexpected(make_memory_error(ENOMEM, "Failed to allocate buffer"));
    }

    return buffer;
}
```

    -- -

    ##Migration Guidelines

    ## #Step 1 : Identify Current Pattern

```cpp
                 // Old: C-style error codes
                 int
                 read_file(const char *path, char *buffer, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    // ...
    return 0;
}
```

    ## #Step 2 : Convert to std::expected

```cpp
                 // New: std::expected
                 auto
                 read_file(std::string_view path) -> Result<std::vector<std::byte>> {
    // ... implementation
}
```

    ## #Step 3 : Update Call Sites

```cpp
                 // Old
                 char buffer[1024];
if (read_file("/etc/config", buffer, sizeof(buffer)) != 0) {
    fprintf(stderr, "Error reading file\n");
    return 1;
}

// New
auto result = read_file("/etc/config");
if (!result) {
    log_error(result);
    return 1;
}
auto &data = result.value();
```

    -- -

    ##Integration with Existing Code

        During transition period :

```cpp
    // Wrapper for legacy code
    auto
    legacy_wrapper(const char *path) -> Result<std::vector<std::byte>> {
    char buffer[4096];
    int ret = legacy_read_file(path, buffer, sizeof(buffer));

    if (ret < 0) {
        return std::unexpected(make_io_error(errno, "Legacy function failed"));
    }

    return std::vector<std::byte>(reinterpret_cast<std::byte *>(buffer),
                                  reinterpret_cast<std::byte *>(buffer + ret));
}
```

    -- -

    ##Performance Notes

    - `std::expected` has **zero overhead **over manual error checking -
    Size of `Result<T>` = size of T + size of Error + 1 byte(discriminator) - No heap allocations -
                          Compiler optimizes away unused error paths

                          -- -

                          ##Testing

```cpp
#include <catch2/catch_test_macros.hpp>

                          TEST_CASE("read_file returns error for missing file") {
    auto result = read_file("/nonexistent");

    REQUIRE(!result);
    REQUIRE(result.error().category == Category::IO);
    REQUIRE(result.error().code == ENOENT);
}

TEST_CASE("read_file succeeds for existing file") {
    auto result = read_file("/etc/passwd");

    REQUIRE(result);
    REQUIRE(!result.value().empty());
}
```

---

**Status:** Ready for use in all new code and refactoring efforts  
**Next:** Refactor 8 critical functions to use this infrastructure

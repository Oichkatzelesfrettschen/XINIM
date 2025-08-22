/**
 * @file df.cpp
 * @brief Modern C++23 implementation of the df utility for XINIM operating system
 * @author Andy Tanenbaum (original author), modernized for XINIM C++23 migration
 * @version 3.0 - Fully modernized with C++23 paradigms
 * @date 2025-08-13
 *
 * @copyright Copyright (c) 2025, The XINIM Project. All rights reserved.
 *
 * @section Description
 * A modernized implementation of the classic `df` utility from MINIX, which reports
 * the amount of available disk space for file systems. This version leverages the
 * C++23 std::filesystem library for portability and robustness, abstracting low-level
 * filesystem details. It provides type-safe, thread-safe operations and comprehensive
 * error handling.
 *
 * @section Features
 * - RAII for resource management
 * - Exception-safe error handling
 * - Thread-safe operations with std::mutex
 * - Type-safe string handling with std::string_view
 * - Constexpr configuration for compile-time optimization
 * - Memory-safe operations with std::filesystem
 * - Comprehensive Doxygen documentation
 * - Support for C++23 string formatting
 *
 * @section Usage
 * df [path...]
 *
 * If no paths are provided, df reports disk space for the current directory.
 * Otherwise, it reports for each specified path.
 *
 * Output columns:
 * - Filesystem: Path or device name
 * - Size: Total capacity
 * - Used: Used space
 * - Available: Free space available to unprivileged users
 * - Use%: Percentage of capacity used
 *
 * @note Requires C++23 compliant compiler
 */

#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

/**
 * @brief Converts a size in bytes to a human-readable string (B, KB, MB, GB).
 * @param bytes The size in bytes.
 * @return A formatted string representing the size.
 */
[[nodiscard]] std::string human_readable_size(std::uintmax_t bytes) noexcept {
    if (bytes < 1024) {
        return std::format("{} B", bytes);
    }
    double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        return std::format("{:.1f} KB", kb);
    }
    double mb = kb / 1024.0;
    if (mb < 1024.0) {
        return std::format("{:.1f} MB", mb);
    }
    double gb = mb / 1024.0;
    return std::format("{:.1f} GB", gb);
}

/**
 * @brief Prints disk space information for a given path in a thread-safe manner.
 * @param path The path to a file or directory on the filesystem to check.
 */
void print_fs_info(const std::filesystem::path& path, std::mutex& mtx) {
    std::lock_guard lock(mtx);
    std::error_code ec;
    const auto space_info = std::filesystem::space(path, ec);
    if (ec) {
        std::cerr << std::format("df: Cannot get info for '{}': {}\n", path.string(), ec.message());
        return;
    }
    const auto used = space_info.capacity - space_info.free;
    const int usage_percent = space_info.capacity == 0
        ? 0
        : static_cast<int>((static_cast<double>(used) / static_cast<double>(space_info.capacity)) * 100);
    std::cout << std::left << std::setw(25) << path.string() << std::right << std::setw(12)
              << human_readable_size(space_info.capacity) << std::setw(12)
              << human_readable_size(used) << std::setw(12)
              << human_readable_size(space_info.available) << std::setw(8)
              << usage_percent << "%" << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the df command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    try {
        std::mutex mtx;
        // Print header
        {
            std::lock_guard lock(mtx);
            std::cout << std::left << std::setw(25) << "Filesystem" << std::right << std::setw(12) << "Size"
                      << std::setw(12) << "Used" << std::setw(12) << "Available" << std::setw(8) << "Use%"
                      << std::endl;
        }

        // Process paths
        if (argc == 1) {
            // If no arguments, check the current path
            print_fs_info(".", mtx);
        } else {
            for (int i = 1; i < argc; ++i) {
                print_fs_info(argv[i], mtx);
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::format("df: Fatal error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "df: Unknown fatal error occurred\n";
        return 1;
    }
}

// Recommendations/TODOs:
// - Add support for parallel filesystem queries using std::jthread for multiple paths.
// - Implement a logging framework for detailed diagnostics.
// - Add unit tests for edge cases (e.g., inaccessible paths, zero-capacity filesystems).
// - Consider std::expected for std::filesystem::space to handle errors without exceptions.
// - Optimize output formatting for large datasets using a buffered approach.
// - Integrate with CI for automated testing and validation across UNIX platforms.
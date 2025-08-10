/**
 * @file df.cpp
 * @brief Display free disk space.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `df` utility from MINIX.
 * It reports the amount of available disk space for file systems.
 * The modern implementation leverages the C++ std::filesystem library for a portable
 * and robust
 * solution, abstracting away the low-level details of specific filesystem structures.
 *
 * Usage: df [path...]
 */

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

/**
 * @brief Converts a size in bytes to a human-readable string (KB, MB, GB).
 * @param bytes The size in bytes.
 * @return A formatted string representing the size.
 */
std::string human_readable_size(std::uintmax_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << kb << " KB";
        return ss.str();
    }
    double mb = kb / 1024.0;
    if (mb < 1024.0) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << mb << " MB";
        return ss.str();
    }
    double gb = mb / 1024.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << gb << " GB";
    return ss.str();
}

/**
 * @brief Prints the disk space information for a given path.
 * @param path The path to a file or directory on the filesystem to check.
 */
void print_fs_info(const std::filesystem::path &path) {
    std::error_code ec;
    const auto space_info = std::filesystem::space(path, ec);

    if (ec) {
        std::cerr << "df: Cannot get info for '" << path.string() << "': " << ec.message()
                  << std::endl;
        return;
    }

    std::cout << std::left << std::setw(25) << path.string() << std::right << std::setw(12)
              << human_readable_size(space_info.capacity) << std::setw(12)
              << human_readable_size(space_info.capacity - space_info.free) << std::setw(12)
              << human_readable_size(space_info.available) << std::setw(8)
              << static_cast<int>((static_cast<double>(space_info.capacity - space_info.free) /
                                   space_info.capacity) *
                                  100)
              << "%" << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the df command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
    std::cout << std::left << std::setw(25) << "Filesystem" << std::right << std::setw(12) << "Size"
              << std::setw(12) << "Used" << std::setw(12) << "Available" << std::setw(8) << "Use%"
              << std::endl;

    if (argc == 1) {
        // If no arguments, check the current path.
        print_fs_info(".");
    } else {
        for (int i = 1; i < argc; ++i) {
            print_fs_info(argv[i]);
        }
    }

    return 0;
}

/**
 * @file cp.cpp
 * @brief Copy files and directories.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `cp` utility from MINIX.
 * It copies files and directories, handling both file-to-file and multiple files
 * to a directory copy operations. It leverages the C++ std::filesystem library for
 * robust and platform-independent file operations and uses `std::println`
 * \[C++23 §20.21] alongside `std::filesystem` \[C++23 §27.12] for modern,
 * type-safe resource management.
 *
 * Usage:
 *   cp source_file target_file
 *   cp source_file... target_directory
 */

#include <filesystem>
#include <iostream>
#include <print> // For std::println
#include <string>
#include <system_error>
#include <vector>

namespace {

/**
 * @brief Prints the usage message to standard error.
 * @note Uses `std::println` \[C++23 §20.21] for type-safe formatted output.
 */
void printUsage() {
    std::println(std::cerr, "Usage: cp source_file target_file");
    std::println(std::cerr, "       cp source_file... target_directory");
}

/**
 * @brief Copies a single source to a target.
 * @param source The source path.
 * @param target The target path.
 * @param is_target_dir A boolean indicating if the target is a directory.
 * @return True on success, false on failure.
 * @note Relies on `std::filesystem::copy` \[C++23 §27.12.15] to perform
 *       recursive copies while honoring RAII for resource safety.
 */
bool copy_item(const std::filesystem::path &source, const std::filesystem::path &target,
               bool is_target_dir) {
    try {
        std::filesystem::path destination = target;
        if (is_target_dir) {
            destination /= source.filename();
        }

        // Check for self-copy
        if (std::filesystem::exists(destination) &&
            std::filesystem::equivalent(source, destination)) {
            std::println(std::cerr, "cp: '{}' and '{}' are the same file", source.string(),
                         destination.string());
            return false;
        }

        // The copy_options::overwrite_existing will handle the case where the destination file
        // exists.
        std::filesystem::copy(source, destination,
                              std::filesystem::copy_options::overwrite_existing |
                                  std::filesystem::copy_options::recursive);
    } catch (const std::filesystem::filesystem_error &e) {
        std::println(std::cerr, "cp: error copying '{}' to '{}': {}", source.string(),
                     destination.string(), e.what());
        return false;
    }
    return true;
}

} // namespace

/**
 * @brief Main entry point for the cp command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 * @note Demonstrates structured bindings and `std::println` \[C++23 §20.21]
 *       for informative diagnostics.
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::vector<std::filesystem::path> sources;
    for (int i = 1; i < argc - 1; ++i) {
        sources.emplace_back(argv[i]);
    }
    std::filesystem::path target(argv[argc - 1]);

    bool target_is_directory = std::filesystem::is_directory(target);

    if (sources.size() > 1 && !target_is_directory) {
        std::println(std::cerr, "cp: target '{}' is not a directory", target.string());
        return 1;
    }

    int status = 0;
    for (const auto &source : sources) {
        if (!copy_item(source, target, target_is_directory)) {
            status = 1; // Indicate that at least one error occurred
        }
    }

    return status;
}

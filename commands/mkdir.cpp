/**
 * @file mkdir.cpp
 * @brief Create directories - C++23 modernized version
 * @details A modern C++23 implementation of the `mkdir` utility
 *          using std::filesystem. Supports creating parent directories.
 *          Integrates with xinim::fs for directory creation.
 */

#include <iostream>      // For std::cerr
#include <vector>        // For std::vector
#include <string>        // For std::string
#include <string_view>   // For std::string_view
#include <filesystem>    // For std::filesystem::*
#include <system_error>  // For std::error_code, std::errc
#include <print>         // For std::println (C++23)
#include <cstdlib>       // For EXIT_SUCCESS, EXIT_FAILURE

#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context

namespace { // Anonymous namespace for helper functions

/**
 * @brief Prints the usage message to standard error.
 */
void print_usage() {
    std::println(std::cerr, "Usage: mkdir [-p] directory...");
}

/**
 * @brief Creates a single directory entry, optionally with parent directories.
 * @param dir_path The path of the directory to create.
 * @param create_parents_flag True if -p (create parents) option is enabled.
 * @return True if the directory was created successfully or already existed (and is a directory),
 *         false on actual error.
 */
bool create_single_directory_entry(
    const std::filesystem::path& dir_path,
    bool create_parents_flag) {

    xinim::fs::operation_context ctx; // Create default context
    // Future: if mkdir gets options for mode (direct/standard) or symlink handling for path components,
    // they would be set in ctx here based on parsed command-line options.
    // ctx.execution_mode = xinim::fs::mode::direct; // Example

    // Revised pre-check
    std::error_code pre_check_ec;
    // For this pre-check, std::filesystem::status is fine.
    // If dir_path is a symlink, status() follows it. This is usually desired:
    // if `symlink -> existing_dir`, mkdir `symlink` should succeed.
    // if `symlink -> existing_file`, mkdir `symlink` should fail.
    auto status = std::filesystem::status(dir_path, pre_check_ec);
    if (!pre_check_ec && std::filesystem::exists(status)) {
        if (!std::filesystem::is_directory(status)) {
            std::println(std::cerr, "mkdir: cannot create directory '{}': File exists and is not a directory", dir_path.string());
            return false;
        }
        // If it exists and IS a directory, mkdir considers this a success.
        return true;
    }
    // If stat failed (e.g. permission denied on parent) or path doesn't exist, proceed to creation attempt.
    // The xinim::fs functions will handle these errors.

    if (create_parents_flag) {
        // Default perms for create_directories in xinim::fs is perms::all, which is suitable for mkdir -p
        // where intermediate directories get default permissions and final one can be adjusted.
        // The current create_directories sets final dir perms.
        auto result = xinim::fs::create_directories(dir_path, std::filesystem::perms::all, ctx);
        if (!result) {
            std::println(std::cerr, "mkdir: cannot create directory '{}': {}", dir_path.string(), result.error().message());
            return false;
        }
    } else {
        // Default perms for create_directory is perms::all.
        auto result = xinim::fs::create_directory(dir_path, std::filesystem::perms::all, ctx);
        if (!result) {
            std::println(std::cerr, "mkdir: cannot create directory '{}': {}", dir_path.string(), result.error().message());
            return false;
        }
    }

    return true;
}

} // namespace

/**
 * @brief Main entry point for the mkdir command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char* argv[]) {
    bool create_parents = false;
    std::vector<std::filesystem::path> paths_to_create;
    bool options_ended = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!options_ended && !arg.empty() && arg[0] == '-') {
            if (arg == "--") { // End of options
                options_ended = true;
                continue;
            }
            // Iterate over characters for combined flags like -p
            if (arg.length() > 1 && arg[0] == '-') {
                 for (size_t j = 1; j < arg.length(); ++j) {
                    char flag = arg[j];
                    switch (flag) {
                        case 'p':
                            create_parents = true;
                            break;
                        default:
                            std::println(std::cerr, "mkdir: unknown option -- '{}'", flag);
                            print_usage();
                            return EXIT_FAILURE;
                    }
                }
            } else {
                 paths_to_create.emplace_back(arg); // Treat "-" as a path if not part of a multi-char option
            }
        } else {
            paths_to_create.emplace_back(arg);
        }
    }

    if (paths_to_create.empty()) {
        print_usage();
        return EXIT_FAILURE;
    }

    // xinim::fs::filesystem_ops fs_ops_instance; // Removed
    bool overall_success = true;

    for (const auto& path : paths_to_create) {
        // fs_ops_instance removed from call
        if (!create_single_directory_entry(path, create_parents)) {
            overall_success = false;
        }
    }

    return overall_success ? EXIT_SUCCESS : EXIT_FAILURE;
}

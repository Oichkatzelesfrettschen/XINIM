/**
 * @file rm.cpp
 * @brief Remove files or directories - C++23 modernized version
 * @details A modern C++23 implementation of the `rm` utility.
 *          Supports force, interactive, and recursive options.
 *          Integrates with xinim::fs for file operations.
 */

#include <iostream>      // For std::cin, std::cout, std::cerr
#include <vector>        // For std::vector
#include <string>        // For std::string
#include <string_view>   // For std::string_view
#include <filesystem>    // For std::filesystem::*
#include <system_error>  // For std::error_code, std::errc
#include <print>         // For std::print, std::println (C++23)
#include <cstdlib>       // For EXIT_SUCCESS, EXIT_FAILURE

#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context

namespace { // Anonymous namespace for helper functions

/**
 * @brief Prints the usage message to standard error.
 */
void print_usage() {
    std::println(std::cerr, "Usage: rm [-firR] file...");
}

/**
 * @brief Asks the user for confirmation before removing a file.
 * @param path_to_remove The path of the file/directory to be removed.
 * @return True if the user confirms (y/Y), false otherwise.
 */
bool ask_confirmation(const std::filesystem::path& path_to_remove) {
    std::print(std::cout, "rm: remove '{}'? ", path_to_remove.string());
    std::string response;
    std::getline(std::cin, response);
    return (response == "y" || response == "Y");
}

/**
 * @brief Removes a single path (file or directory).
 * @param path_to_remove The path to remove.
 * @param force_op True if -f (force) option is enabled.
 * @param interactive_op True if -i (interactive) option is enabled.
 * @param recursive_op True if -r/-R (recursive) option is enabled.
 * @return True if the operation was considered successful (or error suppressed by -f), false on reported error.
 */
bool remove_single_path(
    const std::filesystem::path& path_to_remove,
    bool force_op,
    bool interactive_op,
    bool recursive_op) { // fs_ops parameter removed

    xinim::fs::operation_context ctx;
    // For the initial status check, we want to know the type of the path itself (e.g. if it's a symlink)
    // before deciding on recursive removal or how `remove` vs `remove_all` would treat it.
    // `get_status`'s `follow_symlinks` in `ctx` defaults to true. For rm, usually we want to
    // operate on the link if it's a link, unless -R is following it into a directory.
    // The `xinim::fs::remove` and `xinim::fs::remove_all` use `std::filesystem::remove/remove_all`
    // which operate on the link itself if the path is a symlink.
    // So, for `get_status` here, `follow_symlinks = false` is appropriate to check the type of `path_to_remove`.
    ctx.follow_symlinks = false;

    auto status_result = xinim::fs::get_status(path_to_remove, ctx);

    if (!status_result) {
        bool report_error = true;
        if (force_op) {
            if (status_result.error() == std::errc::no_such_file_or_directory) {
                report_error = false;
            }
        }
        if (report_error) {
            std::println(std::cerr, "rm: cannot remove '{}': {}", path_to_remove.string(), status_result.error().message());
        }
        return force_op ? true : false;
    }

    const auto& item_status = status_result.value();
    if (!item_status.is_populated) {
         if(!force_op) std::println(std::cerr, "rm: could not get valid status for '{}'", path_to_remove.string());
         return force_op ? true : false;
    }

    if (interactive_op) {
        if (!ask_confirmation(path_to_remove)) {
            return true;
        }
    }

    // Reset context for actual remove operations; follow_symlinks in ctx is not
    // directly used by remove/remove_all as they have fixed behavior for symlinks (remove the link).
    // execution_mode from default_ctx will be used by the xinim::fs functions.
    xinim::fs::operation_context remove_ctx;
    // remove_ctx.execution_mode = ...; // Could be set from options if rm had such flags

    if (item_status.type == std::filesystem::file_type::directory) {
        if (!recursive_op) {
            if (!force_op) {
                std::println(std::cerr, "rm: cannot remove '{}': Is a directory", path_to_remove.string());
            }
            return force_op ? true : false;
        }

        auto remove_all_res = xinim::fs::remove_all(path_to_remove, remove_ctx);
        if (!remove_all_res) {
            bool report_error = true;
            if (force_op) {
                if (remove_all_res.error() == std::errc::no_such_file_or_directory) { // Should not happen if initial stat passed
                    report_error = false;
                }
            }
            if (report_error) {
                std::println(std::cerr, "rm: cannot remove '{}': {}", path_to_remove.string(), remove_all_res.error().message());
            }
            return force_op ? true : false;
        }
    } else {
        auto remove_res = xinim::fs::remove(path_to_remove, remove_ctx);
        if (!remove_res) {
            bool report_error = true;
            if (force_op) {
                if (remove_res.error() == std::errc::no_such_file_or_directory) { // Should not happen
                    report_error = false;
                }
            }
            if (report_error) {
                std::println(std::cerr, "rm: cannot remove '{}': {}", path_to_remove.string(), remove_res.error().message());
            }
            return force_op ? true : false;
        }
    }

    return true;
}

} // namespace

/**
 * @brief Main entry point for the rm command.
 */
int main(int argc, char* argv[]) {
    bool force_op = false;
    bool interactive_op = false;
    bool recursive_op = false;
    std::vector<std::filesystem::path> paths_to_remove;
    bool options_ended = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!options_ended && !arg.empty() && arg[0] == '-') {
            if (arg == "--") {
                options_ended = true;
                continue;
            }
            if (arg.starts_with("--")) {
                if (arg == "--force") {
                    force_op = true;
                } else if (arg == "--interactive") {
                    interactive_op = true;
                } else if (arg == "--recursive") {
                    recursive_op = true;
                } else {
                    std::println(std::cerr, "rm: unknown option -- '{}'", arg);
                    print_usage();
                    return EXIT_FAILURE;
                }
            } else {
                 for (size_t j = 1; j < arg.length(); ++j) {
                    char flag = arg[j];
                    switch (flag) {
                        case 'f': force_op = true; break;
                        case 'i': interactive_op = true; break;
                        case 'r': case 'R': recursive_op = true; break;
                        default:
                            std::println(std::cerr, "rm: unknown option -- '{}'", flag);
                            print_usage();
                            return EXIT_FAILURE;
                    }
                }
            }
        } else {
            paths_to_remove.emplace_back(arg);
        }
    }

    if (paths_to_remove.empty()) {
        if (force_op && argc > 1) {
             return EXIT_SUCCESS;
        }
        print_usage();
        return EXIT_FAILURE;
    }

    if (force_op) {
        interactive_op = false;
    }

    // xinim::fs::filesystem_ops fs_ops_instance; // Removed
    bool overall_success = true;

    for (const auto& path : paths_to_remove) {
        // fs_ops_instance removed from call
        if (!remove_single_path(path, force_op, interactive_op, recursive_op)) {
            overall_success = false;
        }
    }

    if (force_op) {
        return EXIT_SUCCESS;
    }

    return overall_success ? EXIT_SUCCESS : EXIT_FAILURE;
}

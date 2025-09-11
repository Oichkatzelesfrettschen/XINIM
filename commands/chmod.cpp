// commands/chmod.cpp
// Based on user's exemplary C++23 implementation, modified to use
// external xinim::util::ModeParser and xinim::fs free functions.

#include <iostream>
#include <filesystem>
#include <expected>
#include <string_view>
#include <vector>
#include <algorithm> // For std::ranges::all_of (used by ModeParser, indirectly)
#include <ranges>    // For std::ranges::all_of (C++20)
#include <print>     // For std::println
#include <cstdlib>   // For EXIT_SUCCESS, EXIT_FAILURE
#include <optional>  // For std::optional

#include "xinim/mode_parser.hpp" // The new shared ModeParser
#include "xinim/filesystem.hpp"  // For xinim::fs free functions and operation_context

namespace {
    // Helper function to display usage information
    void print_usage(std::string_view program_name) {
        std::println(std::cerr, "Usage: {} [-R|--recursive] [--help] MODE FILE...", program_name);
        std::println(std::cerr, "Change the mode of each FILE to MODE.");
        std::println(std::cerr, "MODE can be either an octal number (e.g., 755) or symbolic (e.g., u+x,g-w)");
        std::println(std::cerr, "\nOptions:");
        std::println(std::cerr, "  -R, --recursive     change files and directories recursively");
        std::println(std::cerr, "      --help          display this help and exit");
        // Add other options as they are implemented, e.g. -h for no-dereference on symlinks if chmod supports it
    }
}

/**
 * @brief Entry point for the chmod utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char* argv[]) {
    if (argc < 3) { // Program name + at least mode + one file = 3
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::vector<std::string_view> args_sv(argv + 1, argv + argc);
    bool recursive = false;
    std::string_view mode_str;
    std::vector<std::filesystem::path> files_to_process;
    bool options_parsed = false;

    for (size_t i = 0; i < args_sv.size(); ++i) {
        const auto& arg = args_sv[i];
        if (!options_parsed && !arg.empty() && arg[0] == '-') {
            if (arg == "-R" || arg == "--recursive") {
                recursive = true;
            } else if (arg == "--help") {
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            } else if (arg == "--") {
                options_parsed = true; // All subsequent args are non-options
            }
            else {
                std::println(std::cerr, "{}: invalid option -- '{}'", argv[0], arg);
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else {
            if (mode_str.empty()) {
                mode_str = arg;
            } else {
                files_to_process.emplace_back(arg);
            }
        }
    }

    if (mode_str.empty()) {
        std::println(std::cerr, "{}: missing mode operand", argv[0]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (files_to_process.empty()) {
        std::println(std::cerr, "{}: missing file operand after mode '{}'", argv[0], mode_str);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    xinim::util::ModeParser parser;
    // xinim::fs::filesystem_ops fs_ops; // Removed
    bool had_error = false;

    std::optional<std::filesystem::perms> octal_perms_to_set;
    bool is_mode_octal = !mode_str.empty() && std::ranges::all_of(mode_str, [](char c){ return c >= '0' && c <= '7';});

    if (is_mode_octal) {
        auto parse_result = parser.parse(mode_str, std::filesystem::perms::none);
        if (!parse_result) {
            std::println(std::cerr, "{}: invalid mode '{}': {}", argv[0], mode_str, parse_result.error());
            return EXIT_FAILURE;
        }
        octal_perms_to_set = *parse_result;
    }

    xinim::fs::operation_context default_ctx; // Default context
    // If chmod gets a -h option for symlinks:
    // default_ctx.follow_symlinks_for_target = !no_dereference_option_set;

    for (const auto& path : files_to_process) {
        std::error_code ec_stat;
        auto top_level_status = std::filesystem::symlink_status(path, ec_stat);

        if (ec_stat || !std::filesystem::exists(top_level_status)) {
            std::println(std::cerr, "{}: cannot access '{}': {}", argv[0], path.string(),
                         ec_stat ? ec_stat.message() : "No such file or directory");
            had_error = true;
            continue;
        }

        std::filesystem::perms perms_to_set_for_path;
        if (is_mode_octal) {
            perms_to_set_for_path = *octal_perms_to_set;
        } else {
            // For symbolic modes, need current perms of the actual file/dir (target if symlink)
            // Use a context that follows symlinks for getting the permissions to modify
            xinim::fs::operation_context stat_ctx = default_ctx;
            stat_ctx.follow_symlinks = true; // Ensure we get target's perms for symbolic context

            auto status_for_perms_res = xinim::fs::get_status(path, stat_ctx);
            if(!status_for_perms_res) {
                 std::println(std::cerr, "{}: cannot get current permissions for '{}': {}", argv[0], path.string(), status_for_perms_res.error().message());
                 had_error = true;
                 continue;
            }
            auto parse_result = parser.parse(mode_str, status_for_perms_res.value().permissions);
            if (!parse_result) {
                std::println(std::cerr, "{}: invalid symbolic mode '{}' for '{}': {}", argv[0], mode_str, path.string(), parse_result.error());
                had_error = true;
                continue;
            }
            perms_to_set_for_path = *parse_result;
        }

        // Apply permissions to the current path item
        // The `change_permissions` API no longer takes preserve_special_bits.
        // The ModeParser result is the final, complete permission set.
        auto result = xinim::fs::change_permissions(path, perms_to_set_for_path, default_ctx);
        if (!result) {
            // Error message is now expected to be printed by the xinim::fs layer if it's a direct call,
            // or by the change_permissions function itself. For chmod, we'll print it here too for clarity.
            std::println(std::cerr, "{}: changing permissions of '{}': {}", argv[0], path.string(), result.error().message());
            had_error = true;
        }

        if (recursive && std::filesystem::is_directory(top_level_status)) {
            std::error_code ec_iter;
            auto rdi = std::filesystem::recursive_directory_iterator(
                path,
                std::filesystem::directory_options::skip_permission_denied,
                ec_iter);

            if (ec_iter) {
                std::println(std::cerr, "{}: error accessing directory '{}' for recursion: {}", argv[0], path.string(), ec_iter.message());
                had_error = true;
                continue;
            }

            for (const auto& entry : rdi) {
                std::filesystem::perms perms_for_entry;
                if (is_mode_octal) {
                    perms_for_entry = *octal_perms_to_set;
                } else {
                    xinim::fs::operation_context entry_stat_ctx = default_ctx;
                    entry_stat_ctx.follow_symlinks = true; // Follow links to get target perms

                    auto entry_status_res = xinim::fs::get_status(entry.path(), entry_stat_ctx);
                     if (!entry_status_res) {
                         std::println(std::cerr, "{}: cannot get current permissions for '{}': {}", argv[0], entry.path().string(), entry_status_res.error().message());
                         had_error = true;
                         continue;
                    }
                    auto entry_parse_result = parser.parse(mode_str, entry_status_res.value().permissions);
                    if (!entry_parse_result) {
                        std::println(std::cerr, "{}: invalid symbolic mode '{}' for '{}': {}", argv[0], mode_str, entry.path().string(), entry_parse_result.error());
                        had_error = true;
                        continue;
                    }
                    perms_for_entry = *entry_parse_result;
                }

                // For recursive entries, use the same context as the top-level path.
                // If chmod -hR is ever implemented, entry_ctx.follow_symlinks would be set to false.
                xinim::fs::operation_context entry_ctx = default_ctx;
                auto entry_result = xinim::fs::change_permissions(entry.path(), perms_for_entry, entry_ctx);
                if (!entry_result) {
                    std::println(std::cerr, "{}: changing permissions of '{}': {}", argv[0], entry.path().string(), entry_result.error().message());
                    had_error = true;
                }
            }
        }
    }
    return had_error ? EXIT_FAILURE : EXIT_SUCCESS;
}

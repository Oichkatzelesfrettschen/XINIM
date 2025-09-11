/**
 * @file ln.cpp
 * @brief Create links between files - C++23 modernized version
 * @details A modern C++23 implementation of the `ln` utility.
 *          Supports symbolic and hard links, force option, and linking
 *          multiple files into a directory. Integrates with xinim::fs.
 */

#include <iostream>      // For std::cerr
#include <vector>        // For std::vector
#include <string>        // For std::string
#include <string_view>   // For std::string_view
#include <filesystem>    // For std::filesystem::*
#include <system_error>  // For std::error_code, std::errc
#include <optional>      // For std::optional (not directly used but good for utils)
#include <print>         // For std::println (C++23)
#include <cstdlib>       // For EXIT_SUCCESS, EXIT_FAILURE

#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context

namespace { // Anonymous namespace for helper functions

struct LinkOptions {
    bool symbolic = false;
    bool force = false;
    bool no_target_directory = false; // Corresponds to -T
    // Other options like -i (interactive), -v (verbose) could be added
};

/**
 * @brief Prints the usage message to standard error.
 */
void print_usage(std::string_view program_name) {
    std::println(std::cerr, "Usage: {} [OPTION]... [-T] TARGET LINK_NAME", program_name);
    std::println(std::cerr, "  or:  {} [OPTION]... TARGET", program_name);
    std::println(std::cerr, "  or:  {} [OPTION]... TARGET... DIRECTORY", program_name);
    std::println(std::cerr, "Create links. By default, hard links are made.");
    std::println(std::cerr, "\nOptions:");
    std::println(std::cerr, "  -s, --symbolic        make symbolic links instead of hard links");
    std::println(std::cerr, "  -f, --force           remove existing destination files");
    std::println(std::cerr, "  -T, --no-target-directory  treat LINK_NAME as a normal file always");
    std::println(std::cerr, "      --help          display this help and exit");
}

/**
 * @brief Creates a link for a single item.
 * @param source_target The path the link should point to (for symlink) or the existing file (for hardlink).
 * @param actual_link_path The path where the link file itself should be created.
 * @param options The linking options.
 * @param program_name Name of the program for error messages.
 * @return True on success, false on failure.
 */
bool create_link_for_item( // fs_ops parameter removed
    const std::filesystem::path& source_target,
    const std::filesystem::path& actual_link_path,
    const LinkOptions& options,
    std::string_view program_name) {

    xinim::fs::operation_context ctx; // Default context
    // ctx.execution_mode = xinim::fs::mode::auto_detect; // Default
    // ctx.follow_symlinks = true; // Default, not directly used by link creation functions.

    if (options.force) {
        std::error_code ec_exists;
        // Use symlink_status to check for any type of entry at the destination, including a symlink.
        if (std::filesystem::symlink_status(actual_link_path, ec_exists).type() != std::filesystem::file_type::not_found) {
            auto rm_res = xinim::fs::remove(actual_link_path, ctx);
            if (!rm_res) {
                 if (rm_res.error() != std::errc::no_such_file_or_directory) {
                    std::println(std::cerr, "{}: cannot remove existing destination '{}': {}",
                                 program_name, actual_link_path.string(), rm_res.error().message());
                    return false;
                 }
            }
        } else if (ec_exists && ec_exists != std::errc::no_such_file_or_directory) {
            // Error trying to check status, other than "not found"
             std::println(std::cerr, "{}: cannot access destination '{}': {}",
                                 program_name, actual_link_path.string(), ec_exists.message());
            return false;
        }
    }

    if (options.symbolic) {
        auto sym_res = xinim::fs::create_symlink(source_target, actual_link_path, ctx);
        if (!sym_res) {
            std::println(std::cerr, "{}: cannot create symbolic link '{}' to '{}': {}",
                         program_name, actual_link_path.string(), source_target.string(), sym_res.error().message());
            return false;
        }
    } else { // Hard link
        auto hard_res = xinim::fs::create_hard_link(source_target, actual_link_path, ctx);
        if (!hard_res) {
            std::println(std::cerr, "{}: cannot create hard link '{}' to '{}': {}",
                         program_name, actual_link_path.string(), source_target.string(), hard_res.error().message());
            return false;
        }
    }
    return true;
}

} // anonymous namespace

/**
 * @brief Main entry point for the ln command.
 */
int main(int argc, char* argv[]) {
    std::string_view program_name = argv[0];
    if (argc < 2) { // Needs at least "ln TARGET" (which implies LINK_NAME in CWD) or "ln TARGET LINK_NAME"
        print_usage(program_name);
        return EXIT_FAILURE;
    }

    LinkOptions options;
    std::vector<std::filesystem::path> path_args;
    bool options_ended = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!options_ended && !arg.empty() && arg[0] == '-') {
            if (arg == "--") {
                options_ended = true;
                continue;
            }
            if (arg.starts_with("--")) {
                if (arg == "--symbolic") options.symbolic = true;
                else if (arg == "--force") options.force = true;
                else if (arg == "--no-target-directory") options.no_target_directory = true;
                else if (arg == "--help") { print_usage(program_name); return EXIT_SUCCESS; }
                else {
                    std::println(std::cerr, "{}: unrecognized option '{}'", program_name, arg);
                    print_usage(program_name);
                    return EXIT_FAILURE;
                }
            } else {
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 's': options.symbolic = true; break;
                        case 'f': options.force = true; break;
                        case 'T': options.no_target_directory = true; break;
                        default:
                            std::println(std::cerr, "{}: invalid option -- '{}'", program_name, arg[j]);
                            print_usage(program_name);
                            return EXIT_FAILURE;
                    }
                }
            }
        } else {
            path_args.emplace_back(arg);
        }
    }

    if (path_args.empty()) {
        std::println(std::cerr, "{}: missing file operand", program_name);
        print_usage(program_name);
        return EXIT_FAILURE;
    }
    // If only one path_arg and it's effectively consumed by mode_str logic in other utilities,
    // it means "missing file operand". For ln, if one path_arg, it's TARGET.
    if (path_args.size() == 1 && options.no_target_directory) {
         std::println(std::cerr, "{}: missing destination file operand after '{}'", program_name, path_args[0].string());
         print_usage(program_name);
         return EXIT_FAILURE;
    }


    // xinim::fs::filesystem_ops fs_ops; // Removed
    bool overall_success = true;

    if (path_args.size() == 1) {
        std::filesystem::path target = path_args[0];
        std::filesystem::path link_name = target.filename();
        if (link_name.empty() || link_name == "." || link_name == "..") {
             std::println(std::cerr, "{}: creating link for '{}' requires explicit link name or for target to have a valid filename", program_name, target.string());
             return EXIT_FAILURE;
        }
        // fs_ops removed from call
        if (!create_link_for_item(target, link_name, options, program_name)) {
            overall_success = false;
        }
    } else if (path_args.size() >= 2) {
        std::filesystem::path last_arg = path_args.back();
        // Check if last argument is a directory using std::filesystem::is_directory
        // This requires that the path actually exists to be true.
        // If creating into a dir that might not exist, that's an error for ln.
        std::error_code ec_is_dir;
        bool last_is_existing_dir = std::filesystem::is_directory(last_arg, ec_is_dir);
        // Ignore ec_is_dir for now, if it's not a dir, it'll be treated as LINK_NAME or fail in create_link_for_item

        bool treat_last_as_dir = !options.no_target_directory && last_is_existing_dir;

        if (treat_last_as_dir) { // TARGET... DIRECTORY
            std::filesystem::path dest_dir = last_arg;
            for (size_t i = 0; i < path_args.size() - 1; ++i) {
                std::filesystem::path target = path_args[i];
                std::filesystem::path link_name_in_dir = dest_dir / target.filename();
                if (target.filename().empty() || target.filename() == "." || target.filename() == "..") {
                     std::println(std::cerr, "{}: cannot make link for '{}' in directory '{}': invalid source filename", program_name, target.string(), dest_dir.string());
                     overall_success = false; continue;
                }
                // fs_ops removed from call
                if (!create_link_for_item(target, link_name_in_dir, options, program_name)) {
                    overall_success = false;
                }
            }
        } else if (path_args.size() == 2) { // TARGET LINK_NAME
            std::filesystem::path target = path_args[0];
            std::filesystem::path link_name = path_args[1];
            // fs_ops removed from call
            if (!create_link_for_item(target, link_name, options, program_name)) {
                overall_success = false;
            }
        } else { // More than 2 path args, but last is not a directory (or -T specified)
             std::println(std::cerr, "{}: target '{}' is not a directory, or too many arguments", program_name, last_arg.string());
             print_usage(program_name);
             return EXIT_FAILURE;
        }
    }
    // else path_args.empty() is caught earlier

    return overall_success ? EXIT_SUCCESS : EXIT_FAILURE;
}

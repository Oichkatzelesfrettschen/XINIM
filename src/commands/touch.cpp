/**
 * @file touch.cpp
 * @brief Change file timestamps or create empty files - C++23 modernized version
 * @details A modern C++23 implementation of the `touch` utility.
 *          Supports -a, -m, -c, -h options.
 *          Integrates with xinim::fs for file operations.
 */

#include <iostream>      // For std::cerr
#include <vector>        // For std::vector
#include <string>        // For std::string
#include <string_view>   // For std::string_view
#include <filesystem>    // For std::filesystem::*
#include <system_error>  // For std::error_code, std::errc
#include <optional>      // For std::optional
#include <chrono>        // For std::chrono::*
#include <print>         // For std::println (C++23)
#include <cstdlib>       // For EXIT_SUCCESS, EXIT_FAILURE

#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context

namespace { // Anonymous namespace for helper functions

/**
 * @brief Prints the usage message to standard error.
 */
void print_usage(std::string_view program_name) {
    std::println(std::cerr, "Usage: {} [OPTION]... FILE...", program_name);
    std::println(std::cerr, "Update the access and modification times of each FILE to the current time.");
    std::println(std::cerr, "A FILE argument that does not exist is created empty, unless -c is supplied.");
    std::println(std::cerr, "\nOptions:");
    std::println(std::cerr, "  -a         change only the access time");
    std::println(std::cerr, "  -m         change only the modification time");
    std::println(std::cerr, "  -c, --no-create  do not create any files");
    std::println(std::cerr, "  -h, --no-dereference affect symbolic links instead of their referents");
    std::println(std::cerr, "      --help     display this help and exit");
}

// Helper to convert system_clock::time_point from file_status_ex to file_clock::time_point for set_file_times
std::filesystem::file_time_type to_file_clock_time(const std::chrono::system_clock::time_point& sys_tp) {
    // This conversion assumes file_clock and system_clock might have different epochs or granularities.
    // The specific conversion was part of set_file_times_hybrid's internal logic,
    // but since we are now passing file_time_type, we might need to do it here.
    // However, set_file_times takes std::filesystem::file_time_type, which IS file_clock::time_point.
    // So, if file_status_ex.mtime is system_clock::time_point, we need conversion.
    // The conversion logic: file_time = sys_time - sys_epoch + file_epoch
    // This simplifies to: file_time_point_cast<file_clock::duration>(sys_tp.time_since_epoch())
    // assuming epochs are the same (Jan 1 1970 for both on POSIX).
    // More robustly:
    return std::chrono::time_point_cast<std::filesystem::file_time_type::clock::duration>(
        sys_tp - std::chrono::system_clock::now() + std::filesystem::file_time_type::clock::now()
    );
}


/**
 * @brief Touches a single path (file or directory).
 * @param path The path to touch.
 * @param change_atime True if only access time should be changed to now (or both if change_mtime is also true).
 * @param change_mtime True if only modification time should be changed to now (or both if change_atime is also true).
 * @param no_create_flag True if -c (no-create) option is enabled.
 * @param no_dereference_flag True if -h (no-dereference) option is enabled for symlinks.
 * @param program_name Name of the program for error messages.
 * @return True on success, false on error.
 */
bool do_touch(
    const std::filesystem::path& path,
    bool change_atime,
    bool change_mtime,
    bool no_create_flag,
    bool no_dereference_flag,
    std::string_view program_name) {

    xinim::fs::operation_context ctx;
    ctx.follow_symlinks = !no_dereference_flag; // For set_file_times and initial get_status if it's a symlink

    auto status_res = xinim::fs::get_status(path, ctx);

    if (!status_res) { // Path does not exist or other error getting status
        if (status_res.error() == std::errc::no_such_file_or_directory) {
            if (no_create_flag) {
                return true; // -c: do not create, not an error if file doesn't exist
            }
            // Create the file
            // Default perms 0666 (before umask) for new files by create_file
            xinim::fs::operation_context create_ctx; // Use default context for create_file
            auto create_res = xinim::fs::create_file(path, static_cast<std::filesystem::perms>(0666), false, create_ctx);
            if (!create_res) {
                std::println(std::cerr, "{}: cannot create '{}': {}", program_name, path.string(), create_res.error().message());
                return false;
            }
            // File created. Timestamps are implicitly "now".
            // If -a or -m was specified, POSIX touch still sets both to now for a new file.
            // Our set_file_times with nullopt for both also means "now", so effectively no further action needed on times for new file.
            return true;
        } else {
            // Other error getting status (e.g., permission denied on parent directory)
            std::println(std::cerr, "{}: cannot touch '{}': {}", program_name, path.string(), status_res.error().message());
            return false;
        }
    }

    // Path exists, proceed to update times
    const auto& xinim_status = status_res.value();

    std::optional<std::filesystem::file_time_type> atime_final_opt;
    std::optional<std::filesystem::file_time_type> mtime_final_opt;

    // If neither -a nor -m, or if both -a and -m, update both times to now (by passing nullopt to set_file_times).
    // If only -a: update atime to now (nullopt), keep mtime as is (pass current mtime).
    // If only -m: update mtime to now (nullopt), keep atime as is (pass current atime).

    bool only_atime = change_atime && !change_mtime;
    bool only_mtime = !change_atime && change_mtime;
    // bool both_or_neither = (change_atime && change_mtime) || (!change_atime && !change_mtime); // This is the default for set_file_times

    if (only_atime) {
        // atime_final_opt remains nullopt (set to UTIME_NOW by set_file_times)
        mtime_final_opt = to_file_clock_time(xinim_status.mtime);
    } else if (only_mtime) {
        // mtime_final_opt remains nullopt (set to UTIME_NOW by set_file_times)
        atime_final_opt = to_file_clock_time(xinim_status.atime);
    } else { // Both change or neither specified (means both to now)
        // Both atime_final_opt and mtime_final_opt remain nullopt
    }

    // The context for set_file_times should respect the no_dereference_flag for its operation on the path.
    xinim::fs::operation_context time_ctx;
    time_ctx.follow_symlinks = !no_dereference_flag;
    // time_ctx.execution_mode = ... // Could be set from options if available

    auto times_res = xinim::fs::set_file_times(path, atime_final_opt, mtime_final_opt, time_ctx);
    if (!times_res) {
        std::println(std::cerr, "{}: cannot touch '{}': {}", program_name, path.string(), times_res.error().message());
        return false;
    }

    return true;
}

} // namespace

/**
 * @brief Main entry point for the touch command.
 */
int main(int argc, char* argv[]) {
    std::string_view program_name = argv[0];
    if (argc < 2) {
        print_usage(program_name);
        return EXIT_FAILURE;
    }

    bool change_atime = false;
    bool change_mtime = false;
    bool no_create_flag = false;
    bool no_dereference_flag = false; // For -h
    std::vector<std::filesystem::path> paths_to_touch;
    bool options_ended = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!options_ended && !arg.empty() && arg[0] == '-') {
            if (arg == "--") {
                options_ended = true;
                continue;
            }
            if (arg.starts_with("--")) { // Long options
                if (arg == "--no-create") no_create_flag = true;
                else if (arg == "--no-dereference") no_dereference_flag = true;
                else if (arg == "--help") { print_usage(program_name); return EXIT_SUCCESS; }
                // Note: POSIX touch doesn't have long options for -a, -m usually
                else {
                    std::println(std::cerr, "{}: unrecognized option '{}'", program_name, arg);
                    print_usage(program_name);
                    return EXIT_FAILURE;
                }
            } else { // Short options
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'a': change_atime = true; break;
                        case 'm': change_mtime = true; break;
                        case 'c': no_create_flag = true; break;
                        case 'h': no_dereference_flag = true; break;
                        default:
                            std::println(std::cerr, "{}: invalid option -- '{}'", program_name, arg[j]);
                            print_usage(program_name);
                            return EXIT_FAILURE;
                    }
                }
            }
        } else {
            paths_to_touch.emplace_back(arg);
        }
    }

    if (paths_to_touch.empty()) {
        std::println(std::cerr, "{}: missing file operand", program_name);
        print_usage(program_name);
        return EXIT_FAILURE;
    }

    // If neither -a nor -m is specified, both atime and mtime are changed.
    // If only one is specified, standard touch behavior is to change that one to now,
    // and the other one also to now *unless* -r or -t is used to specify a reference time.
    // Since we don't support -r or -t yet, if only -a or only -m is given,
    // the current logic for set_file_times (passing nullopt for the other) will set both to UTIME_NOW.
    // To strictly change only one and preserve the other, we would need to pass the current value
    // of the other time, as implemented in the `do_touch` refined logic.
    // The boolean flags `change_atime` and `change_mtime` are now directly used to determine
    // which times to affect, with the refined logic in `do_touch`.
    // If neither -a nor -m are given, this implies both should be updated to "now".
    if (!change_atime && !change_mtime) {
        change_atime = true;
        change_mtime = true;
    }


    bool overall_success = true;
    for (const auto& path : paths_to_touch) {
        if (!do_touch(path, change_atime, change_mtime, no_create_flag, no_dereference_flag, program_name)) {
            overall_success = false;
        }
    }

    return overall_success ? EXIT_SUCCESS : EXIT_FAILURE;
}

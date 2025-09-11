/**
 * @file chown.cpp
 * @brief Change owner and group of files - C++23 modernized version
 * @details A modern C++23 implementation of the `chown` utility.
 *          Supports recursive operation and user:group specification.
 *          Integrates with xinim::fs for ownership changes and status retrieval.
 */

#include <iostream>      // For std::cerr, std::cout
#include <vector>        // For std::vector
#include <string>        // For std::string
#include <string_view>   // For std::string_view
#include <filesystem>    // For std::filesystem::*
#include <system_error>  // For std::error_code, std::errc
#include <optional>      // For std::optional
#include <algorithm>     // For std::find (not strictly needed with string_view::find)
#include <charconv>      // For std::from_chars
#include <print>         // For std::println (C++23)
#include <cstdlib>       // For EXIT_SUCCESS, EXIT_FAILURE
#include <cstring>       // For strerror

// POSIX headers for user/group lookup and chown
#include <pwd.h>         // For getpwnam, getpwuid
#include <grp.h>         // For getgrnam
#include <sys/types.h>   // For uid_t, gid_t (already in xinim/filesystem.hpp)
#include <unistd.h>      // For chown, lchown (used by xinim::fs layer for now)
// #include <sys/stat.h> // No longer needed directly by process_path_for_chown as get_status is used

#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context

namespace { // Anonymous namespace for helper functions

/**
 * @brief Prints the usage message to standard error.
 */
void print_usage() {
    std::println(std::cerr, "Usage: chown [-R|--recursive] [-h|--no-dereference] OWNER[:GROUP] file...");
}

/**
 * @brief Parses the OWNER[:GROUP] specification string.
 * @param spec_str The specification string.
 * @return A pair of strings: {owner_name, group_name}. Parts can be empty.
 */
std::pair<std::string, std::string> parse_owner_group_spec(std::string_view spec_str) {
    std::string owner_part;
    std::string group_part;

    auto colon_pos = spec_str.find(':');
    if (colon_pos == std::string_view::npos) {
        owner_part = std::string(spec_str);
    } else {
        owner_part = std::string(spec_str.substr(0, colon_pos));
        if (colon_pos + 1 < spec_str.length()) {
            group_part = std::string(spec_str.substr(colon_pos + 1));
        }
    }
    return {owner_part, group_part};
}

/**
 * @brief Gets UID from a username string or numeric UID string.
 */
std::optional<uid_t> get_uid_from_name(std::string_view name) {
    if (name.empty()) return std::nullopt;
    uid_t numeric_uid;
    auto [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), numeric_uid);
    if (ec == std::errc() && ptr == name.data() + name.size()) {
        return numeric_uid;
    }
    std::string name_str(name);
    if (struct passwd *pw = getpwnam(name_str.c_str())) {
        return pw->pw_uid;
    }
    return std::nullopt;
}

/**
 * @brief Gets GID from a group name string or numeric GID string.
 */
std::optional<gid_t> get_gid_from_name(std::string_view name) {
    if (name.empty()) return std::nullopt;
    gid_t numeric_gid;
    auto [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), numeric_gid);
    if (ec == std::errc() && ptr == name.data() + name.size()) {
        return numeric_gid;
    }
    std::string name_str(name);
    if (struct group *gr = getgrnam(name_str.c_str())) {
        return gr->gr_gid;
    }
    return std::nullopt;
}

/**
 * @brief Gets the primary GID for a given UID.
 */
std::optional<gid_t> get_primary_gid_for_uid(uid_t uid) {
    if (struct passwd *pw = getpwuid(uid)) {
        return pw->pw_gid;
    }
    return std::nullopt;
}

/**
 * @brief Applies owner and group to a path using xinim::fs layer.
 */
bool apply_chown(
    const std::filesystem::path& p,
    uid_t final_uid,
    gid_t final_gid,
    const xinim::fs::operation_context& ctx) { // Takes operation_context

    auto result = xinim::fs::change_ownership(p, final_uid, final_gid, ctx);
    if (!result) {
        std::println(std::cerr, "chown: cannot change owner/group of '{}': {}", p.string(), result.error().message());
        return false;
    }
    return true;
}

/**
 * @brief Processes a single path entry (file or directory) for chown.
 */
bool process_path_for_chown(
    const std::filesystem::path& current_path,
    std::optional<uid_t> target_uid_opt,
    std::optional<gid_t> target_gid_opt,
    bool is_recursive,
    bool actual_follow_symlinks_policy) { // fs_ops parameter removed

    uid_t final_uid = static_cast<uid_t>(-1);
    gid_t final_gid = static_cast<gid_t>(-1);

    xinim::fs::operation_context status_ctx;
    status_ctx.follow_symlinks = false; // Mimic lstat for initial check

    auto status_result = xinim::fs::get_status(current_path, status_ctx);
    if (!status_result) {
        std::println(std::cerr, "chown: cannot access '{}': {}", current_path.string(), status_result.error().message());
        return false;
    }
    const auto& xinim_status = status_result.value();
    if (!xinim_status.is_populated) {
        std::println(std::cerr, "chown: failed to get valid status for '{}'", current_path.string());
        return false;
    }

    if (target_uid_opt) {
        final_uid = *target_uid_opt;
    } else {
        final_uid = xinim_status.uid;
    }

    if (target_gid_opt) {
        final_gid = *target_gid_opt;
    } else {
        final_gid = xinim_status.gid;
    }

    xinim::fs::operation_context apply_chown_ctx;
    apply_chown_ctx.follow_symlinks = actual_follow_symlinks_policy;
    // apply_chown_ctx.execution_mode could be set here if chown had specific mode flags

    bool success = apply_chown(current_path, final_uid, final_gid, apply_chown_ctx);

    if (is_recursive && xinim_status.type == std::filesystem::file_type::directory) {
        std::error_code ec_iter;
        auto rdi = std::filesystem::recursive_directory_iterator(
            current_path,
            std::filesystem::directory_options::skip_permission_denied,
            ec_iter
        );

        if (ec_iter) {
            std::println(std::cerr, "chown: error accessing directory '{}' for recursion: {}", current_path.string(), ec_iter.message());
            return false;
        }

        for (const auto& entry : rdi) {
            // For recursive entries, the follow_symlinks policy for the chown operation itself is applied.
            // recursive_directory_iterator follows symlinks to directories by default.
            if (!apply_chown(entry.path(), final_uid, final_gid, apply_chown_ctx)) {
                success = false;
            }
        }
    }
    return success;
}

} // namespace

/**
 * @brief Main entry point for the chown command.
 */
int main(int argc, char* argv[]) {
    bool recursive_opt = false;
    bool follow_symlinks = true;
    std::string owner_group_spec_str;
    std::vector<std::filesystem::path> file_paths;
    bool options_ended = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!options_ended && !arg.empty() && arg[0] == '-') {
            if (arg == "--") {
                options_ended = true;
                continue;
            }
            if (arg.starts_with("--")) {
                if (arg == "--recursive") {
                    recursive_opt = true;
                } else if (arg == "--no-dereference") {
                    follow_symlinks = false;
                } else {
                    std::println(std::cerr, "chown: unknown option -- '{}'", arg);
                    print_usage();
                    return EXIT_FAILURE;
                }
            } else {
                 for (size_t j = 1; j < arg.length(); ++j) {
                    char flag = arg[j];
                    switch (flag) {
                        case 'R':
                            recursive_opt = true;
                            break;
                        case 'h':
                            follow_symlinks = false;
                            break;
                        default:
                            std::println(std::cerr, "chown: unknown option -- '{}'", std::string(1,flag));
                            print_usage();
                            return EXIT_FAILURE;
                    }
                }
            }
        } else {
            if (owner_group_spec_str.empty()) {
                owner_group_spec_str = std::string(arg);
            } else {
                file_paths.emplace_back(arg);
            }
        }
    }

    if (owner_group_spec_str.empty() || file_paths.empty()) {
        print_usage();
        return EXIT_FAILURE;
    }

    auto [owner_name, group_name] = parse_owner_group_spec(owner_group_spec_str);

    std::optional<uid_t> target_uid_opt;
    std::optional<gid_t> target_gid_opt;

    if (!owner_name.empty()) {
        target_uid_opt = get_uid_from_name(owner_name);
        if (!target_uid_opt) {
            std::println(std::cerr, "chown: invalid user: '{}'", owner_name);
            return EXIT_FAILURE;
        }
    }

    bool explicit_group_sep = (owner_group_spec_str.find(':') != std::string_view::npos);

    if (!group_name.empty()) {
        target_gid_opt = get_gid_from_name(group_name);
        if (!target_gid_opt) {
            std::println(std::cerr, "chown: invalid group: '{}'", group_name);
            return EXIT_FAILURE;
        }
    } else if (explicit_group_sep) {
        if (target_uid_opt) {
            target_gid_opt = get_primary_gid_for_uid(*target_uid_opt);
            if (!target_gid_opt) {
                 std::println(std::cerr, "chown: cannot get primary group for user '{}'", owner_name);
                 return EXIT_FAILURE;
            }
        } else {
             std::println(std::cerr, "chown: invalid spec: ':' alone requires a user, or a group must be specified after ':'");
             return EXIT_FAILURE;
        }
    }

    if (!target_uid_opt && !target_gid_opt) {
         std::println(std::cerr, "chown: must specify owner and/or group, or an error occurred resolving them.");
         return EXIT_FAILURE;
    }

    // xinim::fs::filesystem_ops fs_ops_instance; // Removed
    bool overall_success = true;

    for (const auto& path : file_paths) {
        // fs_ops_instance removed from call
        if (!process_path_for_chown(path, target_uid_opt, target_gid_opt, recursive_opt, follow_symlinks)) {
            overall_success = false;
        }
    }

    return overall_success ? EXIT_SUCCESS : EXIT_FAILURE;
}

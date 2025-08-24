#include "xinim/filesystem.hpp" // Main header for this library's declarations

#include <sys/stat.h>  // For POSIX mode_t, S_IF*, ::mkdir, ::chmod, ::stat, ::lstat, utimensat constants
#include <cerrno>      // For errno
#include <unistd.h>    // For POSIX syscalls: ::chmod, ::chown, ::lchown, ::stat, ::lstat, ::unlink, ::rmdir, ::symlink, ::link, ::readlink, ::open, ::close, ::rename
#include <sys/types.h> // For POSIX types like uid_t, gid_t (often included by other POSIX headers)
#include <chrono>      // For std::chrono types used in file_status_ex and time conversions
#include <string>      // For std::string (e.g., readlink buffer)
#include <vector>      // Not directly used in final version of readlink buffer, but often useful
#include <fcntl.h>     // For ::open flags (O_CREAT, O_WRONLY, O_EXCL)
#include <time.h>      // For struct timespec, UTIME_NOW (for utimensat)
#include <cstdio>      // For ::rename (standard C header for rename)
#include <fstream>     // For std::ofstream in create_file (standard path)


namespace xinim::fs {

// --- Internal Helper Functions ---

// Determines if direct OS calls should be preferred based on context.
bool should_use_direct_os_call(const std::filesystem::path& /*path*/, const operation_context& ctx) {
    if (ctx.execution_mode == mode::direct) {
        return true;
    }
    if (ctx.execution_mode == mode::standard) {
        return false;
    }
    // auto_detect logic:
    // Defaulting to false (prefer std::filesystem) for most operations unless they
    // inherently require direct calls (like get_status, change_ownership) or specific XINIM features.
    return false;
}

namespace { // Anonymous namespace for file-local static helper functions

// Converts std::filesystem::perms to POSIX mode_t.
mode_t to_posix_mode(std::filesystem::perms p) {
    mode_t modeval = 0;
    if ((p & std::filesystem::perms::owner_read) != std::filesystem::perms::none) modeval |= S_IRUSR;
    if ((p & std::filesystem::perms::owner_write) != std::filesystem::perms::none) modeval |= S_IWUSR;
    if ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) modeval |= S_IXUSR;
    if ((p & std::filesystem::perms::group_read) != std::filesystem::perms::none) modeval |= S_IRGRP;
    if ((p & std::filesystem::perms::group_write) != std::filesystem::perms::none) modeval |= S_IWGRP;
    if ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none) modeval |= S_IXGRP;
    if ((p & std::filesystem::perms::others_read) != std::filesystem::perms::none) modeval |= S_IROTH;
    if ((p & std::filesystem::perms::others_write) != std::filesystem::perms::none) modeval |= S_IWOTH;
    if ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none) modeval |= S_IXOTH;
    if ((p & std::filesystem::perms::set_uid) != std::filesystem::perms::none) modeval |= S_ISUID;
    if ((p & std::filesystem::perms::set_gid) != std::filesystem::perms::none) modeval |= S_ISGID;
    if ((p & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none) modeval |= S_ISVTX;
    return modeval;
}

// Converts POSIX mode_t to std::filesystem::file_type.
std::filesystem::file_type posix_mode_to_file_type(mode_t posix_mode) {
    if (S_ISREG(posix_mode)) return std::filesystem::file_type::regular;
    if (S_ISDIR(posix_mode)) return std::filesystem::file_type::directory;
    if (S_ISLNK(posix_mode)) return std::filesystem::file_type::symlink;
    if (S_ISBLK(posix_mode)) return std::filesystem::file_type::block;
    if (S_ISCHR(posix_mode)) return std::filesystem::file_type::character;
    if (S_ISFIFO(posix_mode)) return std::filesystem::file_type::fifo;
    if (S_ISSOCK(posix_mode)) return std::filesystem::file_type::socket;
    return std::filesystem::file_type::unknown;
}

// Converts POSIX mode_t to std::filesystem::perms.
std::filesystem::perms posix_mode_to_filesystem_perms(mode_t posix_mode) {
    std::filesystem::perms p = std::filesystem::perms::none;
    if (posix_mode & S_IRUSR) p |= std::filesystem::perms::owner_read;
    if (posix_mode & S_IWUSR) p |= std::filesystem::perms::owner_write;
    if (posix_mode & S_IXUSR) p |= std::filesystem::perms::owner_exec;
    if (posix_mode & S_IRGRP) p |= std::filesystem::perms::group_read;
    if (posix_mode & S_IWGRP) p |= std::filesystem::perms::group_write;
    if (posix_mode & S_IXGRP) p |= std::filesystem::perms::group_exec;
    if (posix_mode & S_IROTH) p |= std::filesystem::perms::others_read;
    if (posix_mode & S_IWOTH) p |= std::filesystem::perms::others_write;
    if (posix_mode & S_IXOTH) p |= std::filesystem::perms::others_exec;
    if (posix_mode & S_ISUID) p |= std::filesystem::perms::set_uid;
    if (posix_mode & S_ISGID) p |= std::filesystem::perms::set_gid;
    if (posix_mode & S_ISVTX) p |= std::filesystem::perms::sticky_bit;
    return p;
}

} // anonymous namespace

// --- Forward declaration for copy ---
std::expected<void, std::error_code> copy(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::filesystem::copy_options options,
    const operation_context& ctx);

// --- Public API Implementations ---

std::expected<file_status_ex, std::error_code> get_status(
    const std::filesystem::path& path,
    const operation_context& ctx) {
    struct stat statbuf;
    int result = ctx.follow_symlinks ? ::stat(path.c_str(), &statbuf) : ::lstat(path.c_str(), &statbuf);
    if (result != 0) return std::unexpected(std::error_code(errno, std::system_category()));

    file_status_ex status_ex_result;
    status_ex_result.type = posix_mode_to_file_type(statbuf.st_mode);
    status_ex_result.permissions = posix_mode_to_filesystem_perms(statbuf.st_mode);
    status_ex_result.uid = statbuf.st_uid;
    status_ex_result.gid = statbuf.st_gid;
    status_ex_result.file_size = static_cast<std::uintmax_t>(statbuf.st_size);
    status_ex_result.link_count = static_cast<nlink_t>(statbuf.st_nlink);
    status_ex_result.device = statbuf.st_dev;
    status_ex_result.inode = statbuf.st_ino;
    status_ex_result.rdevice = statbuf.st_rdev;
    status_ex_result.mtime = std::chrono::system_clock::from_time_t(statbuf.st_mtime);
    status_ex_result.atime = std::chrono::system_clock::from_time_t(statbuf.st_atime);
    status_ex_result.ctime = std::chrono::system_clock::from_time_t(statbuf.st_ctime);
    status_ex_result.is_populated = true;
    return status_ex_result;
}

std::expected<void, std::error_code> create_directory(
    const std::filesystem::path& path,
    std::filesystem::perms prms,
    const operation_context& ctx) {
    if (should_use_direct_os_call(path, ctx)) {
        mode_t mode_val = to_posix_mode(prms);
        if (::mkdir(path.c_str(), mode_val) != 0) {
            if (errno == EEXIST) {
                operation_context stat_ctx = ctx;
                stat_ctx.follow_symlinks = false;
                auto current_status_res = get_status(path, stat_ctx);
                if (current_status_res && current_status_res.value().type == std::filesystem::file_type::directory) {
                    return {};
                }
            }
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        return {};
    } else {
        std::error_code ec;
        std::filesystem::create_directory(path, ec);
        if (ec) {
            if (ec == std::errc::file_exists) {
                 std::error_code stat_ec;
                 auto current_status = std::filesystem::symlink_status(path, stat_ec);
                 if (!stat_ec && std::filesystem::is_directory(current_status)) {
                     return {};
                 }
            }
            return std::unexpected(ec);
        }
        return {};
    }
}

std::expected<void, std::error_code> create_directories(
    const std::filesystem::path& path,
    std::filesystem::perms prms_for_final_dir,
    const operation_context& ctx) {
    operation_context pre_check_ctx = ctx;
    pre_check_ctx.follow_symlinks = true;
    auto status_res = get_status(path, pre_check_ctx);

    if (status_res && status_res.value().is_populated) {
        if (status_res.value().type != std::filesystem::file_type::directory) {
            return std::unexpected(std::make_error_code(std::errc::file_exists));
        }
    } else if (status_res.error() != std::errc::no_such_file_or_directory) {
        return std::unexpected(status_res.error());
    }

    std::error_code ec_create;
    std::filesystem::create_directories(path, ec_create);
    if (ec_create) {
        // Check if it failed because it already exists as a directory (race condition or concurrent creation)
        if (ec_create == std::errc::file_exists) {
            operation_context check_dir_ctx = ctx; check_dir_ctx.follow_symlinks = true;
            auto post_create_status = get_status(path, check_dir_ctx);
            if (post_create_status && post_create_status.value().type == std::filesystem::file_type::directory) {
                // It's fine, directory exists. Proceed to set permissions.
            } else {
                 return std::unexpected(ec_create); // Still an error
            }
        } else {
            return std::unexpected(ec_create); // Other creation error
        }
    }

    auto perm_result = xinim::fs::change_permissions(path, prms_for_final_dir, ctx);
    if (!perm_result) {
        return std::unexpected(perm_result.error());
    }
    return {};
}

std::expected<void, std::error_code> change_permissions(
    const std::filesystem::path& path,
    std::filesystem::perms perms,
    const operation_context& ctx) {
    if (should_use_direct_os_call(path, ctx)) {
        mode_t mode_val = to_posix_mode(perms);
        int res = -1;
        if (!ctx.follow_symlinks) {
            #if defined(AT_SYMLINK_NOFOLLOW) && defined(__linux__)
            res = ::fchmodat(AT_FDCWD, path.c_str(), mode_val, AT_SYMLINK_NOFOLLOW);
            #elif defined(LCHMOD_SUPPORTED)
            res = ::lchmod(path.c_str(), mode_val);
            #else
            return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
            #endif
        } else {
             res = ::chmod(path.c_str(), mode_val);
        }
        if (res != 0) {
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        return {};
    } else {
        std::error_code ec;
        auto opts = std::filesystem::perm_options::replace;
        if (!ctx.follow_symlinks) {
            opts |= std::filesystem::perm_options::nofollow;
        }
        std::filesystem::permissions(path, perms, opts, ec);
        if (ec) {
            return std::unexpected(ec);
        }
        return {};
    }
}

std::expected<void, std::error_code> change_ownership(
    const std::filesystem::path& path,
    ::uid_t uid,
    ::gid_t gid,
    const operation_context& ctx) {
    bool use_direct = (ctx.execution_mode == mode::direct);
    if (ctx.execution_mode == mode::auto_detect) {
        use_direct = true;
    }
    if (use_direct) {
        int result = -1;
        if (ctx.follow_symlinks) {
            result = ::chown(path.c_str(), uid, gid);
        } else {
            #if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 1) || defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700
                result = ::lchown(path.c_str(), uid, gid);
            #else
                if (!ctx.follow_symlinks) {
                    return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
                }
                result = ::chown(path.c_str(), uid, gid);
            #endif
        }
        if (result != 0) return std::unexpected(std::error_code(errno, std::system_category()));
        return {};
    } else {
        return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    }
}

std::expected<void, std::error_code> remove(
    const std::filesystem::path& path,
    const operation_context& ctx) {
    if (should_use_direct_os_call(path, ctx)) {
        operation_context status_ctx = ctx;
        status_ctx.follow_symlinks = false;
        auto status_result = get_status(path, status_ctx);
        if (!status_result) return std::unexpected(status_result.error());

        int ret = -1;
        if (status_result.value().type == std::filesystem::file_type::directory) {
            ret = ::rmdir(path.c_str());
        } else {
            ret = ::unlink(path.c_str());
        }
        if (ret != 0) return std::unexpected(std::error_code(errno, std::system_category()));
        return {};
    } else {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) return std::unexpected(ec);
        return {};
    }
}

std::expected<std::uintmax_t, std::error_code> remove_all(
    const std::filesystem::path& path,
    const operation_context& ctx) {
    should_use_direct_os_call(path, ctx);
    std::error_code ec;
    std::uintmax_t count = std::filesystem::remove_all(path, ec);
    if (ec) return std::unexpected(ec);
    return count;
}

std::expected<void, std::error_code> create_symlink(
    const std::filesystem::path& target,
    const std::filesystem::path& link,
    const operation_context& ctx) {
    if (should_use_direct_os_call(link, ctx)) {
        if (::symlink(target.string().c_str(), link.c_str()) != 0) {
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        return {};
    } else {
        std::error_code ec;
        std::filesystem::create_symlink(target, link, ec);
        if (ec) return std::unexpected(ec);
        return {};
    }
}

std::expected<void, std::error_code> create_hard_link(
    const std::filesystem::path& target,
    const std::filesystem::path& link,
    const operation_context& ctx) {
    if (should_use_direct_os_call(link, ctx)) {
        if (::link(target.c_str(), link.c_str()) != 0) {
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        return {};
    } else {
        std::error_code ec;
        std::filesystem::create_hard_link(target, link, ec);
        if (ec) return std::unexpected(ec);
        return {};
    }
}

std::expected<std::filesystem::path, std::error_code> read_symlink(
    const std::filesystem::path& path,
    const operation_context& ctx) {
    if (should_use_direct_os_call(path, ctx)) {
        std::string buffer_str;
        ssize_t len = -1;
        size_t buffer_size = 256;
        constexpr size_t max_symlink_buffer_size = 4096;
        do {
            buffer_str.resize(buffer_size);
            len = ::readlink(path.c_str(), buffer_str.data(), buffer_str.size());
            if (len == -1) return std::unexpected(std::error_code(errno, std::system_category()));
            if (static_cast<size_t>(len) < buffer_str.size()) {
                buffer_str.resize(static_cast<size_t>(len));
                return std::filesystem::path(buffer_str);
            }
            if (buffer_size >= max_symlink_buffer_size) return std::unexpected(std::make_error_code(std::errc::filename_too_long));
            buffer_size *= 2;
            if (buffer_size > max_symlink_buffer_size) buffer_size = max_symlink_buffer_size;
        } while (static_cast<size_t>(len) == buffer_str.size());
        return std::unexpected(std::make_error_code(std::errc::io_error));
    } else {
        std::error_code ec;
        std::filesystem::path target_path = std::filesystem::read_symlink(path, ec);
        if (ec) return std::unexpected(ec);
        return target_path;
    }
}

std::expected<void, std::error_code> set_file_times(
    const std::filesystem::path& path,
    const std::optional<std::filesystem::file_time_type>& access_time_opt,
    const std::optional<std::filesystem::file_time_type>& modification_time_opt,
    const operation_context& ctx) {
    struct timespec ts[2];
    if (access_time_opt) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(*access_time_opt - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto tt = std::chrono::system_clock::to_time_t(sctp);
        auto nsec_chrono = std::chrono::duration_cast<std::chrono::nanoseconds>(sctp.time_since_epoch());
        ts[0].tv_sec = tt;
        ts[0].tv_nsec = nsec_chrono.count() % 1000000000L;
    } else { ts[0].tv_nsec = UTIME_NOW; }

    if (modification_time_opt) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(*modification_time_opt - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto tt = std::chrono::system_clock::to_time_t(sctp);
        auto nsec_chrono = std::chrono::duration_cast<std::chrono::nanoseconds>(sctp.time_since_epoch());
        ts[1].tv_sec = tt;
        ts[1].tv_nsec = nsec_chrono.count() % 1000000000L;
    } else { ts[1].tv_nsec = UTIME_NOW; }

    if (!access_time_opt && !modification_time_opt) {
        ts[0].tv_nsec = UTIME_NOW; ts[1].tv_nsec = UTIME_NOW;
    }

    should_use_direct_os_call(path, ctx);

    int flags = 0;
    #if defined(AT_SYMLINK_NOFOLLOW)
    if (!ctx.follow_symlinks) flags |= AT_SYMLINK_NOFOLLOW;
    #else
    if (!ctx.follow_symlinks) return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    #endif

    if (::utimensat(AT_FDCWD, path.c_str(), ts, flags) != 0) {
        return std::unexpected(std::error_code(errno, std::system_category()));
    }
    return {};
}

std::expected<void, std::error_code> create_file(
    const std::filesystem::path& path,
    std::filesystem::perms prms,
    bool fail_if_exists,
    const operation_context& ctx) {

    operation_context stat_ctx = ctx;
    stat_ctx.follow_symlinks = false;
    auto status_res = get_status(path, stat_ctx);

    if (status_res) {
        if (fail_if_exists) return std::unexpected(std::make_error_code(std::errc::file_exists));
        if (status_res.value().type == std::filesystem::file_type::directory) return std::unexpected(std::make_error_code(std::errc::is_a_directory));
        return {};
    } else {
        if (status_res.error() != std::errc::no_such_file_or_directory) return std::unexpected(status_res.error());
    }

    if (should_use_direct_os_call(path, ctx)) {
        mode_t mode_val = to_posix_mode(prms);
        int open_flags = O_WRONLY | O_CREAT;
        if (fail_if_exists) open_flags |= O_EXCL;

        int fd = ::open(path.c_str(), open_flags, mode_val);
        if (fd == -1) {
            if (errno == EEXIST && !fail_if_exists) {
                 operation_context final_stat_ctx = ctx; final_stat_ctx.follow_symlinks = false;
                 auto final_status_res = get_status(path, final_stat_ctx);
                 if (final_status_res && final_status_res.value().type == std::filesystem::file_type::regular) return {};
            }
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        ::close(fd);
        return {};
    } else {
        std::ofstream outfile(path);
        if (!outfile.is_open()) return std::unexpected(std::make_error_code(std::errc::io_error));
        outfile.close();

        operation_context perm_ctx = ctx;
        perm_ctx.execution_mode = mode::standard;
        perm_ctx.follow_symlinks = true;
        auto perm_result = xinim::fs::change_permissions(path, prms, perm_ctx);
        if (!perm_result) {
            std::error_code remove_ec; std::filesystem::remove(path, remove_ec);
            return std::unexpected(perm_result.error());
        }
        return {};
    }
}

std::expected<void, std::error_code> rename(
    const std::filesystem::path& old_path,
    const std::filesystem::path& new_path,
    const operation_context& ctx) {
    if (should_use_direct_os_call(old_path, ctx)) {
        if (::rename(old_path.c_str(), new_path.c_str()) != 0) {
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        return {};
    } else {
        std::error_code ec;
        std::filesystem::rename(old_path, new_path, ec);
        if (ec) return std::unexpected(ec);
        return {};
    }
}

std::expected<void, std::error_code> copy_file(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::filesystem::copy_options options,
    const operation_context& ctx) {
    should_use_direct_os_call(from, ctx);
    std::error_code ec;
    std::filesystem::copy_file(from, to, options, ec);
    if (ec) return std::unexpected(ec);
    return {};
}

std::expected<void, std::error_code> copy_symlink(
    const std::filesystem::path& from, // The existing symlink
    const std::filesystem::path& to,   // The new symlink to create
    const operation_context& ctx) {

    auto read_target_result = xinim::fs::read_symlink(from, ctx);
    if (!read_target_result) {
        return std::unexpected(read_target_result.error());
    }
    const std::filesystem::path& symlink_points_to = read_target_result.value();

    auto create_link_result = xinim::fs::create_symlink(symlink_points_to, to, ctx);
    if (!create_link_result) {
        return std::unexpected(create_link_result.error());
    }

    return {}; // Success
}

// Main copy function (recursive for directories)
std::expected<void, std::error_code> copy(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::filesystem::copy_options options,
    const operation_context& ctx) {

    xinim::fs::operation_context from_stat_ctx = ctx;
    // If copy_symlinks option is set, we want status of the link itself. Otherwise, status of target.
    from_stat_ctx.follow_symlinks = !static_cast<bool>(options & std::filesystem::copy_options::copy_symlinks);

    auto from_status_res = get_status(from, from_stat_ctx);
    if (!from_status_res) {
        return std::unexpected(from_status_res.error());
    }
    const auto& from_stat = from_status_res.value();
    if (!from_stat.is_populated) {
        return std::unexpected(std::make_error_code(std::errc::io_error));
    }

    // Handle 'to' path existence
    xinim::fs::operation_context to_stat_ctx = ctx;
    to_stat_ctx.follow_symlinks = false; // Check the 'to' path itself, not what it might point to
    auto to_status_res = get_status(to, to_stat_ctx);

    if (to_status_res && to_status_res.value().is_populated) { // 'to' exists
        std::error_code eq_ec;
        // Check if 'from' and 'to' are the same file/directory to prevent self-copy issues.
        // std::filesystem::equivalent needs both paths to exist.
        // If 'from' is a symlink and we are copying its target, use the resolved path for 'from'.
        std::filesystem::path from_for_equivalent_check = from;
        if(from_stat.type != std::filesystem::file_type::symlink && std::filesystem::is_symlink(std::filesystem::symlink_status(from))) { // from was a symlink, and we followed it
            from_for_equivalent_check = std::filesystem::read_symlink(from, eq_ec);
            if (eq_ec) return std::unexpected(eq_ec);
            if(from_for_equivalent_check.is_relative()) from_for_equivalent_check = from.parent_path() / from_for_equivalent_check;
        }

        if (std::filesystem::exists(from_for_equivalent_check) && std::filesystem::exists(to) && std::filesystem::equivalent(from_for_equivalent_check, to, eq_ec) && !eq_ec) {
             return std::unexpected(std::make_error_code(std::errc::file_exists)); // Copying to itself
        }


        if (static_cast<bool>(options & std::filesystem::copy_options::skip_existing)) {
            return {}; // Success, do nothing
        }
        if (static_cast<bool>(options & std::filesystem::copy_options::overwrite_existing) ||
            static_cast<bool>(options & std::filesystem::copy_options::update_existing)) { // update_existing also implies overwrite if source is newer

            // For update_existing, we'd need to compare file times first.
            // This simplified version treats update_existing like overwrite_existing for now if 'to' exists.
            // A full implementation of update_existing would be:
            // if from_stat.mtime <= to_stat_res.value().mtime, return {}; // skip

            auto remove_ctx = ctx;
            std::expected<std::uintmax_t, std::error_code> remove_res_all;
            std::expected<void, std::error_code> remove_res_single;

            if (to_status_res.value().type == std::filesystem::file_type::directory) {
                // Cannot overwrite a directory with a file using std::filesystem::copy_file.
                // std::filesystem::copy with overwrite on a directory means merging or replacing.
                // If 'from' is not a directory, this is an error.
                if (from_stat.type != std::filesystem::file_type::directory && !static_cast<bool>(options & std::filesystem::copy_options::directories_only) ) { // directories_only is not a standard option for this check
                     return std::unexpected(std::make_error_code(std::errc::is_a_directory)); // Trying to overwrite dir with file
                }
                // If both are dirs, overwrite means remove 'to' then copy 'from' into its place, or merge.
                // For now, let's assume `remove_all` is appropriate for `overwrite_existing` on a directory.
                remove_res_all = remove_all(to, remove_ctx);
                if (!remove_res_all) return std::unexpected(remove_res_all.error());
            } else { // 'to' is a file or symlink
                remove_res_single = remove(to, remove_ctx); // remove the file/symlink
                if (!remove_res_single) return std::unexpected(remove_res_single.error());
            }
        } else { // 'to' exists, and no skip_existing or overwrite_existing/update_existing
            return std::unexpected(std::make_error_code(std::errc::file_exists));
        }
    } else {
        if (to_status_res.error() != std::errc::no_such_file_or_directory) {
            return std::unexpected(to_status_res.error());
        }
    }

    // Main copy logic based on 'from' type (which could be the target type if 'from' was a followed symlink)
    if (from_stat.type == std::filesystem::file_type::directory) {
        if ((options & std::filesystem::copy_options::recursive) == std::filesystem::copy_options::none) {
            return std::unexpected(std::make_error_code(std::errc::is_a_directory));
        }

        auto create_dir_ctx = ctx;
        auto create_dir_res = create_directory(to, from_stat.permissions, create_dir_ctx);
        if (!create_dir_res) {
            if (create_dir_res.error() == std::errc::file_exists) {
                operation_context check_to_is_dir_ctx = ctx; check_to_is_dir_ctx.follow_symlinks = true;
                auto to_is_dir_res = get_status(to, check_to_is_dir_ctx);
                if (!to_is_dir_res || to_is_dir_res.value().type != std::filesystem::file_type::directory) {
                    return std::unexpected(create_dir_res.error());
                }
            } else {
                return std::unexpected(create_dir_res.error());
            }
        }

        if ((options & std::filesystem::copy_options::recursive) != std::filesystem::copy_options::none) {
            std::error_code ec_iter;
            for (const auto& entry : std::filesystem::directory_iterator(from, ec_iter)) {
                if (ec_iter) return std::unexpected(ec_iter);

                auto rec_copy_res = copy(entry.path(), to / entry.path().filename(), options, ctx);
                if (!rec_copy_res) {
                    return std::unexpected(rec_copy_res.error());
                }
            }
        }
    } else if (from_stat.type == std::filesystem::file_type::symlink) {
        // This means 'from' itself is a symlink AND copy_symlinks option was set.
        return copy_symlink(from, to, ctx);
    } else if (from_stat.type == std::filesystem::file_type::regular) {
        return copy_file(from, to, options, ctx);
    } else {
        // Other types (sockets, fifo, char/block devices) are not copied by this function.
        return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    }

    return {}; // Success
}

} // namespace xinim::fs

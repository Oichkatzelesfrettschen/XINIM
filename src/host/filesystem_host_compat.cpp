#include <xinim/filesystem.hpp>

#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace xinim::fs {

namespace {

std::error_code current_errno_code(int value = errno) {
    return std::error_code(value, std::generic_category());
}

std::filesystem::perms perms_from_mode(mode_t mode) {
    std::filesystem::perms perms = std::filesystem::perms::none;
    if (mode & S_IRUSR) perms |= std::filesystem::perms::owner_read;
    if (mode & S_IWUSR) perms |= std::filesystem::perms::owner_write;
    if (mode & S_IXUSR) perms |= std::filesystem::perms::owner_exec;
    if (mode & S_IRGRP) perms |= std::filesystem::perms::group_read;
    if (mode & S_IWGRP) perms |= std::filesystem::perms::group_write;
    if (mode & S_IXGRP) perms |= std::filesystem::perms::group_exec;
    if (mode & S_IROTH) perms |= std::filesystem::perms::others_read;
    if (mode & S_IWOTH) perms |= std::filesystem::perms::others_write;
    if (mode & S_IXOTH) perms |= std::filesystem::perms::others_exec;
    if (mode & S_ISUID) perms |= std::filesystem::perms::set_uid;
    if (mode & S_ISGID) perms |= std::filesystem::perms::set_gid;
    if (mode & S_ISVTX) perms |= std::filesystem::perms::sticky_bit;
    return perms;
}

mode_t mode_from_perms(std::filesystem::perms perms) {
    mode_t mode = 0;
    if ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none) mode |= S_IRUSR;
    if ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none) mode |= S_IWUSR;
    if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) mode |= S_IXUSR;
    if ((perms & std::filesystem::perms::group_read) != std::filesystem::perms::none) mode |= S_IRGRP;
    if ((perms & std::filesystem::perms::group_write) != std::filesystem::perms::none) mode |= S_IWGRP;
    if ((perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none) mode |= S_IXGRP;
    if ((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none) mode |= S_IROTH;
    if ((perms & std::filesystem::perms::others_write) != std::filesystem::perms::none) mode |= S_IWOTH;
    if ((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none) mode |= S_IXOTH;
    if ((perms & std::filesystem::perms::set_uid) != std::filesystem::perms::none) mode |= S_ISUID;
    if ((perms & std::filesystem::perms::set_gid) != std::filesystem::perms::none) mode |= S_ISGID;
    if ((perms & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none) mode |= S_ISVTX;
    return mode;
}

std::filesystem::file_type file_type_from_mode(mode_t mode) {
    if (S_ISREG(mode)) return std::filesystem::file_type::regular;
    if (S_ISDIR(mode)) return std::filesystem::file_type::directory;
    if (S_ISLNK(mode)) return std::filesystem::file_type::symlink;
    if (S_ISCHR(mode)) return std::filesystem::file_type::character;
    if (S_ISBLK(mode)) return std::filesystem::file_type::block;
    if (S_ISFIFO(mode)) return std::filesystem::file_type::fifo;
    if (S_ISSOCK(mode)) return std::filesystem::file_type::socket;
    return std::filesystem::file_type::unknown;
}

std::chrono::system_clock::time_point system_time_from_stat_value(std::time_t seconds_value,
                                                                  long nanoseconds_value) {
    using namespace std::chrono;
    const auto duration_value =
        duration_cast<system_clock::duration>(seconds(seconds_value) + nanoseconds(nanoseconds_value));
    return system_clock::time_point(duration_value);
}

std::timespec timespec_from_file_time(const std::filesystem::file_time_type& value) {
    using namespace std::chrono;
    const auto delta = value - std::filesystem::file_time_type::clock::now();
    const auto sys_target = system_clock::now() + duration_cast<system_clock::duration>(delta);
    const auto seconds_point = time_point_cast<seconds>(sys_target);
    const auto nanos = duration_cast<nanoseconds>(sys_target - seconds_point);

    std::timespec ts{};
    ts.tv_sec = static_cast<std::time_t>(seconds_point.time_since_epoch().count());
    ts.tv_nsec = static_cast<long>(nanos.count());
    return ts;
}

int stat_for_path(const std::filesystem::path& path,
                  const operation_context& ctx,
                  struct stat* stat_buffer) {
    if (ctx.follow_symlinks) {
        return ::stat(path.c_str(), stat_buffer);
    }
    return ::lstat(path.c_str(), stat_buffer);
}

std::expected<void, std::error_code> apply_exact_permissions(const std::filesystem::path& path,
                                                             std::filesystem::perms perms,
                                                             const operation_context& ctx) {
    if (!ctx.follow_symlinks) {
        struct stat stat_buffer {};
        if (::lstat(path.c_str(), &stat_buffer) != 0) {
            return std::unexpected(current_errno_code());
        }
        if (S_ISLNK(stat_buffer.st_mode)) {
            return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
        }
    }

    if (::chmod(path.c_str(), mode_from_perms(perms)) != 0) {
        return std::unexpected(current_errno_code());
    }
    return {};
}

} // namespace

std::expected<file_status_ex, std::error_code> get_status(const std::filesystem::path& path,
                                                          const operation_context& ctx) {
    struct stat stat_buffer {};
    if (stat_for_path(path, ctx, &stat_buffer) != 0) {
        return std::unexpected(current_errno_code());
    }

    file_status_ex status {};
    status.type = file_type_from_mode(stat_buffer.st_mode);
    status.permissions = perms_from_mode(stat_buffer.st_mode);
    status.uid = stat_buffer.st_uid;
    status.gid = stat_buffer.st_gid;
    status.file_size = static_cast<std::uintmax_t>(stat_buffer.st_size);
    status.link_count = static_cast<::nlink_t>(stat_buffer.st_nlink);
    status.device = static_cast<::dev_t>(stat_buffer.st_dev);
    status.inode = static_cast<::ino_t>(stat_buffer.st_ino);
    status.rdevice = static_cast<::dev_t>(stat_buffer.st_rdev);
    status.atime = system_time_from_stat_value(stat_buffer.st_atim.tv_sec, stat_buffer.st_atim.tv_nsec);
    status.mtime = system_time_from_stat_value(stat_buffer.st_mtim.tv_sec, stat_buffer.st_mtim.tv_nsec);
    status.ctime = system_time_from_stat_value(stat_buffer.st_ctim.tv_sec, stat_buffer.st_ctim.tv_nsec);
    status.is_populated = true;
    return status;
}

std::expected<void, std::error_code> create_directory(const std::filesystem::path& path,
                                                      std::filesystem::perms prms,
                                                      const operation_context& ctx) {
    std::error_code ec;
    const bool created = std::filesystem::create_directory(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    if (created) {
        auto result = apply_exact_permissions(path, prms, ctx);
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    return {};
}

std::expected<void, std::error_code> create_directories(const std::filesystem::path& path,
                                                        std::filesystem::perms prms_for_final_dir,
                                                        const operation_context& ctx) {
    std::error_code ec;
    const bool created = std::filesystem::create_directories(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    if (created) {
        auto result = apply_exact_permissions(path, prms_for_final_dir, ctx);
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    return {};
}

std::expected<void, std::error_code> change_permissions(const std::filesystem::path& path,
                                                        std::filesystem::perms perms,
                                                        const operation_context& ctx) {
    return apply_exact_permissions(path, perms, ctx);
}

std::expected<void, std::error_code> change_ownership(const std::filesystem::path& path,
                                                      ::uid_t uid,
                                                      ::gid_t gid,
                                                      const operation_context& ctx) {
    if (ctx.execution_mode == mode::standard) {
        return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    }
    const int rc = ctx.follow_symlinks ? ::chown(path.c_str(), uid, gid)
                                       : ::lchown(path.c_str(), uid, gid);
    if (rc != 0) {
        return std::unexpected(current_errno_code());
    }
    return {};
}

std::expected<void, std::error_code> remove(const std::filesystem::path& path,
                                            const operation_context&) {
    std::error_code ec;
    const bool removed = std::filesystem::remove(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    if (!removed) {
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }
    return {};
}

std::expected<std::uintmax_t, std::error_code> remove_all(const std::filesystem::path& path,
                                                          const operation_context&) {
    std::error_code ec;
    const std::uintmax_t removed = std::filesystem::remove_all(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return removed;
}

std::expected<void, std::error_code> create_symlink(const std::filesystem::path& target,
                                                    const std::filesystem::path& link,
                                                    const operation_context&) {
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return {};
}

std::expected<void, std::error_code> create_hard_link(const std::filesystem::path& target,
                                                      const std::filesystem::path& link,
                                                      const operation_context&) {
    std::error_code ec;
    std::filesystem::create_hard_link(target, link, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return {};
}

std::expected<std::filesystem::path, std::error_code> read_symlink(const std::filesystem::path& path,
                                                                   const operation_context&) {
    std::error_code ec;
    std::filesystem::path target = std::filesystem::read_symlink(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return target;
}

std::expected<void, std::error_code> set_file_times(
    const std::filesystem::path& path,
    const std::optional<std::filesystem::file_time_type>& access_time,
    const std::optional<std::filesystem::file_time_type>& modification_time,
    const operation_context& ctx) {
    std::timespec times[2] {};
    if (access_time.has_value()) {
        times[0] = timespec_from_file_time(*access_time);
    } else {
        times[0].tv_nsec = UTIME_NOW;
    }
    if (modification_time.has_value()) {
        times[1] = timespec_from_file_time(*modification_time);
    } else {
        times[1].tv_nsec = UTIME_NOW;
    }

    const int flags = ctx.follow_symlinks ? 0 : AT_SYMLINK_NOFOLLOW;
    if (::utimensat(AT_FDCWD, path.c_str(), times, flags) != 0) {
        return std::unexpected(current_errno_code());
    }
    return {};
}

std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                 std::filesystem::perms prms,
                                                 bool fail_if_exists,
                                                 const operation_context&) {
    struct stat stat_buffer {};
    const bool existed_before = ::lstat(path.c_str(), &stat_buffer) == 0;

    int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
    if (fail_if_exists) {
        flags |= O_EXCL;
    }
    const mode_t mode = mode_from_perms(prms);
    const int fd = ::open(path.c_str(), flags, mode);
    if (fd < 0) {
        return std::unexpected(current_errno_code());
    }
    if (!existed_before && ::fchmod(fd, mode) != 0) {
        const auto ec = current_errno_code();
        ::close(fd);
        return std::unexpected(ec);
    }
    ::close(fd);
    return {};
}

std::expected<void, std::error_code> rename(const std::filesystem::path& old_path,
                                            const std::filesystem::path& new_path,
                                            const operation_context&) {
    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return {};
}

std::expected<void, std::error_code> copy_file(const std::filesystem::path& from,
                                               const std::filesystem::path& to,
                                               std::filesystem::copy_options options,
                                               const operation_context&) {
    std::error_code status_ec;
    const auto source_status = std::filesystem::symlink_status(from, status_ec);
    if (status_ec) {
        return std::unexpected(status_ec);
    }
    if (source_status.type() == std::filesystem::file_type::directory) {
        return std::unexpected(std::make_error_code(std::errc::is_a_directory));
    }

    std::error_code ec;
    const bool copied = std::filesystem::copy_file(from, to, options, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    if (!copied) {
        const bool skip_existing =
            (options & std::filesystem::copy_options::skip_existing) != std::filesystem::copy_options::none;
        if (skip_existing) {
            return {};
        }
        return std::unexpected(std::make_error_code(std::errc::file_exists));
    }
    return {};
}

std::expected<void, std::error_code> copy_symlink(const std::filesystem::path& from,
                                                  const std::filesystem::path& to,
                                                  const operation_context& ctx) {
    auto target = read_symlink(from, ctx);
    if (!target) {
        return std::unexpected(target.error());
    }
    return create_symlink(*target, to, ctx);
}

std::expected<void, std::error_code> copy(const std::filesystem::path& from,
                                          const std::filesystem::path& to,
                                          std::filesystem::copy_options options,
                                          const operation_context&) {
    std::error_code ec;
    std::filesystem::copy(from, to, options, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return {};
}

std::expected<void, std::error_code> set_extended_attr(const std::filesystem::path&,
                                                       std::string_view,
                                                       std::span<const std::byte>,
                                                       const operation_context&) {
    return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
}

} // namespace xinim::fs

/**
 * @file ls.cpp
 * @brief Modern C++23 directory listing utility with advanced file system operations
 * @author XINIM Project (Modernized from Andy Tanenbaum's original MINIX implementation)
 * @version 2.2
 * @date 2024-01-17 (xinim::fs free function integration)
 *
 * A complete paradigmatic modernization of the classic UNIX ls command into
 * pure, cycle-efficient, SIMD/FPU-ready, hardware-agnostic C++23 constructs.
 * Features comprehensive RAII resource management, exception safety, and
 * advanced template metaprogramming for optimal performance.
 * Integrates with xinim::fs free functions for comprehensive file status retrieval.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>    // For std::strerror
#include <ctime>
#include <cerrno>     // For errno
#include <exception>
#include <execution>
#include <filesystem> // For std::filesystem::*
#include <format>
#include <fstream>
#include <iostream>   // For std::cerr, std::cout (used by std::println as default)
#include <memory>
#include <numeric>
#include <optional>
#include <print>      // For std::println (C++23)
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error> // For std::error_code, std::system_category
#include <type_traits>
#include <unordered_map>
#include <vector>

// System headers
#include <sys/stat.h> // For S_IFMT, S_IFDIR etc. and POSIX mode_t constants
#include <dirent.h>   // For DIR, opendir, readdir, closedir (POSIX fallback)
#include <pwd.h>      // For passwd
#include <grp.h>      // For group

#include "xinim/filesystem.hpp" // For xinim::fs operations

// Optional XINIM headers (fallback gracefully if not found)
#ifdef XINIM_HEADERS_AVAILABLE
#include "../fs/const.hpp"
#include "../fs/type.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"
#endif

namespace xinim::commands::ls {

using namespace std::literals;

namespace {
    mode_t to_posix_mode_from_fs_perms(std::filesystem::perms p, std::filesystem::file_type type = std::filesystem::file_type::regular) {
        mode_t modeval = 0;
        switch(type) {
            case std::filesystem::file_type::directory: modeval |= S_IFDIR; break;
            case std::filesystem::file_type::character: modeval |= S_IFCHR; break;
            case std::filesystem::file_type::block:     modeval |= S_IFBLK; break;
            case std::filesystem::file_type::fifo:      modeval |= S_IFIFO; break;
            case std::filesystem::file_type::symlink:   modeval |= S_IFLNK; break;
            case std::filesystem::file_type::socket:    modeval |= S_IFSOCK; break;
            case std::filesystem::file_type::regular:   modeval |= S_IFREG; break;
            default: break;
        }
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
}

namespace simd_ops { /* ... same ... */
    [[nodiscard]] inline int compare_strings_simd(std::string_view lhs, std::string_view rhs) noexcept {
        const auto min_len = std::min(lhs.size(), rhs.size());
        if (min_len >= 32) { // Threshold for SIMD effectiveness, adjust based on architecture
            const auto result = std::memcmp(lhs.data(), rhs.data(), min_len);
            if (result != 0) return (result < 0) ? -1 : 1;
        } else { // Fallback for shorter strings
            for (std::size_t i = 0; i < min_len; ++i) {
                if (lhs[i] != rhs[i]) return (lhs[i] < rhs[i]) ? -1 : 1;
            }
        }
        if (lhs.size() < rhs.size()) return -1;
        if (lhs.size() > rhs.size()) return 1;
        return 0;
    }
    [[nodiscard]] constexpr std::array<std::uint8_t, 3> extract_permission_bits(std::uint16_t mode) noexcept {
        return {
            static_cast<std::uint8_t>((mode >> 6) & 0x7), // User
            static_cast<std::uint8_t>((mode >> 3) & 0x7), // Group
            static_cast<std::uint8_t>(mode & 0x7)         // Other
        };
    }
}

namespace constants { /* ... same ... */
    inline constexpr std::size_t MAX_FILES{256};
    inline constexpr std::int64_t SECONDS_PER_YEAR{365L * 24L * 3600L};
}

class FileInfo final { /* ... same ... */
public:
    using TimePoint = std::chrono::system_clock::time_point;
    using FileSize = std::uintmax_t;
    using InodeNumber = ::ino_t;
    using UserId = ::uid_t;
    using GroupId = ::gid_t;
    using FileMode = ::mode_t;
    using LinkCount = ::nlink_t;

private:
    std::string name_;
    FileMode mode_{0};
    UserId uid_{0};
    GroupId gid_{0};
    InodeNumber inode_{0};
    TimePoint modification_time_{};
    TimePoint access_time_{};
    TimePoint status_change_time_{};
    FileSize size_{0};
    LinkCount link_count_{0};
    bool stat_performed_{false};

public:
    explicit FileInfo(std::string name) noexcept : name_(std::move(name)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] FileMode mode() const noexcept { return mode_; }
    [[nodiscard]] UserId uid() const noexcept { return uid_; }
    [[nodiscard]] GroupId gid() const noexcept { return gid_; }
    [[nodiscard]] InodeNumber inode() const noexcept { return inode_; }
    [[nodiscard]] const TimePoint& modification_time() const noexcept { return modification_time_; }
    [[nodiscard]] const TimePoint& access_time() const noexcept { return access_time_; }
    [[nodiscard]] const TimePoint& status_change_time() const noexcept { return status_change_time_; }
    [[nodiscard]] FileSize size() const noexcept { return size_; }
    [[nodiscard]] LinkCount link_count() const noexcept { return link_count_; }
    [[nodiscard]] bool is_stat_performed() const noexcept { return stat_performed_; }

    void update_from_xinim_status(const xinim::fs::file_status_ex& xinim_status) {
        mode_ = static_cast<FileMode>(to_posix_mode_from_fs_perms(xinim_status.permissions, xinim_status.type));
        uid_ = static_cast<UserId>(xinim_status.uid);
        gid_ = static_cast<GroupId>(xinim_status.gid);
        inode_ = static_cast<InodeNumber>(xinim_status.inode);
        modification_time_ = xinim_status.mtime;
        access_time_ = xinim_status.atime;
        status_change_time_ = xinim_status.ctime;
        if (xinim_status.type == std::filesystem::file_type::character ||
            xinim_status.type == std::filesystem::file_type::block) {
            size_ = static_cast<FileSize>(xinim_status.rdevice);
        } else {
            size_ = static_cast<FileSize>(xinim_status.file_size);
        }
        link_count_ = static_cast<LinkCount>(xinim_status.link_count);
        stat_performed_ = xinim_status.is_populated;
    }

    [[nodiscard]] bool is_directory() const noexcept { return S_ISDIR(mode_); }
    [[nodiscard]] bool is_device() const noexcept { return S_ISCHR(mode_) || S_ISBLK(mode_); }
};

class PermissionFormatter final { /* ... same ... */
private:
    static constexpr std::array permission_strings{ "---"sv, "--x"sv, "-w-"sv, "-wx"sv, "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv };
    static constexpr std::array setuid_strings{ "---"sv, "--x"sv, "-w-"sv, "-wx"sv, "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv, "--S"sv, "--s"sv, "-wS"sv, "-ws"sv, "r-S"sv, "r-s"sv, "rwS"sv, "rws"sv };
    static constexpr std::array setgid_strings{ "---"sv, "--x"sv, "-w-"sv, "-wx"sv, "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv, "--S"sv, "--s"sv, "-wS"sv, "-ws"sv, "r-S"sv, "r-s"sv, "rwS"sv, "rws"sv };
    static constexpr std::array sticky_strings{ "---"sv, "--x"sv, "-w-"sv, "-wx"sv, "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv, "--T"sv, "--t"sv, "-wT"sv, "-wt"sv, "r-T"sv, "r-t"sv, "rwT"sv, "rwt"sv };
public:
    [[nodiscard]] static std::string format_permissions(FileInfo::FileMode mode) noexcept {
        std::string result;
        result.reserve(10);
        result += get_file_type_char(mode);
        const auto perm_bits = simd_ops::extract_permission_bits(static_cast<std::uint16_t>(mode & 0xFFF));
        auto owner_perm = perm_bits[0];
        if (mode & S_ISUID) result += setuid_strings[owner_perm + ((owner_perm & 1) ? 0 : 8)];
        else result += permission_strings[owner_perm];
        auto group_perm = perm_bits[1];
        if (mode & S_ISGID) result += setgid_strings[group_perm + ((group_perm & 1) ? 0 : 8)];
        else result += permission_strings[group_perm];
        auto other_perm = perm_bits[2];
        if (mode & S_ISVTX) result += sticky_strings[other_perm + ((other_perm & 1) ? 0 : 8)];
        else result += permission_strings[other_perm];
        return result;
    }
private:
    [[nodiscard]] static constexpr char get_file_type_char(FileInfo::FileMode mode) noexcept {
        if (S_ISDIR(mode))  return 'd';
        if (S_ISBLK(mode))  return 'b';
        if (S_ISCHR(mode))  return 'c';
        if (S_ISLNK(mode))  return 'l';
        if (S_ISFIFO(mode)) return 'p';
        if (S_ISSOCK(mode)) return 's';
        return '-';
    }
};

enum class ListingFlags : std::uint64_t { /* ... same ... */
    ShowAll       = 1ULL << ('a' - 'a'), ShowBlocks    = 1ULL << ('s' - 'a'), LongFormat    = 1ULL << ('l' - 'a'),
    ShowInodes    = 1ULL << ('i' - 'a'), SortByTime    = 1ULL << ('t' - 'a'), ReverseSort   = 1ULL << ('r' - 'a'),
    NoSort        = 1ULL << ('f' - 'a'), DirectoryOnly = 1ULL << ('d' - 'a'), ShowGroup     = 1ULL << ('g' - 'a'),
    UseAccessTime = 1ULL << ('u' - 'a'), UseChangeTime = 1ULL << ('c' - 'a')
};
constexpr ListingFlags operator|(ListingFlags lhs, ListingFlags rhs) noexcept { return static_cast<ListingFlags>(static_cast<std::underlying_type_t<ListingFlags>>(lhs) | static_cast<std::underlying_type_t<ListingFlags>>(rhs)); }
constexpr ListingFlags operator&(ListingFlags lhs, ListingFlags rhs) noexcept { return static_cast<ListingFlags>(static_cast<std::underlying_type_t<ListingFlags>>(lhs) & static_cast<std::underlying_type_t<ListingFlags>>(rhs)); }

struct UserGroupCache { /* ... same ... */
private:
    mutable std::unordered_map<std::uint16_t, std::string> uid_cache_;
    mutable std::unordered_map<std::uint16_t, std::string> gid_cache_;
    mutable std::optional<std::ifstream> passwd_file_;
    mutable std::optional<std::ifstream> group_file_;
public:
    [[nodiscard]] std::optional<std::string> get_username(std::uint16_t uid) const { if (auto it = uid_cache_.find(uid); it != uid_cache_.end()) return it->second; return load_username(uid); }
    [[nodiscard]] std::optional<std::string> get_groupname(std::uint16_t gid) const { if (auto it = gid_cache_.find(gid); it != gid_cache_.end()) return it->second; return load_groupname(gid); }
private:
    std::optional<std::string> load_username(std::uint16_t uid) const { try { if (!passwd_file_) { passwd_file_ = std::ifstream{"/etc/passwd"}; if (!passwd_file_->is_open()) { passwd_file_.reset(); return std::nullopt; } } passwd_file_->clear(); passwd_file_->seekg(0); std::string line; while (std::getline(*passwd_file_, line)) { const auto tokens = tokenize_passwd_line(line); if (tokens.size() >= 3) { try { if (static_cast<std::uint16_t>(std::stoul(tokens[2])) == uid) { uid_cache_[uid] = tokens[0]; return tokens[0]; } } catch (const std::exception&) { continue; } } } } catch (const std::exception&) { } return std::nullopt; }
    std::optional<std::string> load_groupname(std::uint16_t gid) const { try { if (!group_file_) { group_file_ = std::ifstream{"/etc/group"}; if (!group_file_->is_open()) { group_file_.reset(); return std::nullopt; } } group_file_->clear(); group_file_->seekg(0); std::string line; while (std::getline(*group_file_, line)) { const auto tokens = tokenize_group_line(line); if (tokens.size() >= 3) { try { if (static_cast<std::uint16_t>(std::stoul(tokens[2])) == gid) { gid_cache_[gid] = tokens[0]; return tokens[0]; } } catch (const std::exception&) { continue; } } } } catch (const std::exception&) { } return std::nullopt; }
    [[nodiscard]] static std::vector<std::string> tokenize_passwd_line(const std::string& line) { std::vector<std::string> tokens; std::size_t start = 0; for (std::size_t pos = 0; pos < line.length(); ++pos) { if (line[pos] == ':') { tokens.emplace_back(line.substr(start, pos - start)); start = pos + 1; } } if (start < line.length()) { tokens.emplace_back(line.substr(start)); } return tokens; }
    [[nodiscard]] static std::vector<std::string> tokenize_group_line(const std::string& line) { return tokenize_passwd_line(line); }
};

class DirectoryLister final {
private:
    std::vector<FileInfo> files_;
    std::vector<std::size_t> sort_indices_;
    ListingFlags flags_{};
    UserGroupCache user_cache_;
    std::chrono::system_clock::time_point current_time_;
    int overall_exit_status_{0};
    // xinim::fs::filesystem_ops fs_ops_; // Removed: using free functions now

public:
    explicit DirectoryLister(ListingFlags flags) noexcept
        : flags_(flags), current_time_(std::chrono::system_clock::now()) {
        files_.reserve(constants::MAX_FILES);
        sort_indices_.reserve(constants::MAX_FILES);
    }

    [[nodiscard]] int process_arguments(int argc, char* argv[]) { /* ... same argument parsing ... */
        overall_exit_status_ = 0;
        try {
            const auto parsed_flags = parse_command_line(argc, argv);
            flags_ = parsed_flags.flags;

            if (parsed_flags.file_arguments.empty()) {
                process_single_path(".");
            } else if (parsed_flags.file_arguments.size() == 1) {
                process_single_path(parsed_flags.file_arguments[0]);
            } else {
                process_multiple_paths(parsed_flags.file_arguments);
            }
        } catch (const std::invalid_argument& e) {
            std::println(std::cerr, "ls: {}", e.what());
            return 2; // EXIT_FAILURE or specific code for arg error
        }
        return overall_exit_status_;
    }

private:
    struct ParsedArguments { ListingFlags flags; std::vector<std::string> file_arguments; };
    [[nodiscard]] ParsedArguments parse_command_line(int argc, char* argv[]) const { /* ... same ... */
        ParsedArguments result{}; std::uint64_t flag_bits = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg.starts_with('-') && arg.length() > 1) {
                for (std::size_t j = 1; j < arg.length(); ++j) {
                    const char flag_char = arg[j];
                    const std::string_view valid_flag_chars = "adfgilrstuc";
                    if (valid_flag_chars.find(flag_char) == std::string_view::npos) {
                         throw std::invalid_argument(std::format("invalid option -- '{}'", flag_char));
                    }
                    flag_bits |= (1ULL << (flag_char - 'a'));
                }
            } else { result.file_arguments.emplace_back(arg); }
        }
        result.flags = static_cast<ListingFlags>(flag_bits);
        if (has_flag(result.flags, ListingFlags::NoSort)) result.flags = result.flags | ListingFlags::ShowAll;
        return result;
    }

    void process_single_path(const std::string& path_str) {
        FileInfo initial_file_info(path_str);
        try {
            stat_file(initial_file_info);
        } catch (const std::filesystem::filesystem_error& e) {
            std::println(std::cerr, "ls: cannot access '{}': {} ({})", path_str, e.what(), e.code().message());
            overall_exit_status_ = 1; // EXIT_FAILURE
            // Even if stat fails, if it's a command line arg, we might want to list its name.
            // The original code added it to files_ and then called sort_files_and_print_listing.
            // This behavior is kept.
            files_.emplace_back(std::move(initial_file_info));
            sort_files_and_print_listing();
            return;
        }

        if (initial_file_info.is_directory() && !has_flag(flags_, ListingFlags::DirectoryOnly)) {
            files_.clear();
            sort_indices_.clear();
            expand_directory_impl(path_str);
        } else { // It's a file, or -d was specified for a directory
            files_.clear();
            sort_indices_.clear();
            files_.emplace_back(std::move(initial_file_info));
        }
        sort_files_and_print_listing();
    }

    void process_multiple_paths(const std::vector<std::string>& paths) { /* ... same ... */
        std::vector<FileInfo> file_args; std::vector<FileInfo> dir_args;
        for (const auto& path_str : paths) {
            FileInfo current_file_info(path_str);
            try { stat_file(current_file_info); }
            catch (const std::filesystem::filesystem_error& e) {
                std::println(std::cerr, "ls: cannot access '{}': {} ({})", path_str, e.what(), e.code().message());
                overall_exit_status_ = 1; // EXIT_FAILURE
                // If stat fails for a command line arg, it's often still listed (e.g., with ??? for details).
                // The original code implicitly did this by continuing. We'll add it to file_args to maintain this.
            }
            // Add to appropriate list even if stat failed, so it can be reported as "cannot access"
            if (current_file_info.is_directory() && !has_flag(flags_, ListingFlags::DirectoryOnly) && current_file_info.is_stat_performed()) {
                dir_args.emplace_back(std::move(current_file_info));
            } else {
                file_args.emplace_back(std::move(current_file_info));
            }
        }
        if (!file_args.empty()) { files_ = std::move(file_args); sort_indices_.clear(); sort_files_and_print_listing(); }
        bool first_dir = file_args.empty();
        for (const auto& dir_info : dir_args) {
            if (!first_dir) std::println(std::cout, "");
            first_dir = false;
            std::println(std::cout, "{}:", dir_info.name());
            files_.clear(); sort_indices_.clear();
            expand_directory_impl(dir_info.name());
            sort_files_and_print_listing();
        }
    }

    void sort_files_and_print_listing() { sort_files(); print_listing(); }
    [[nodiscard]] static constexpr bool has_flag(ListingFlags flags, ListingFlags flag) noexcept { return (flags & flag) != static_cast<ListingFlags>(0); }
    [[nodiscard]] bool should_stat_file() const noexcept { return has_flag(flags_, ListingFlags::LongFormat) || has_flag(flags_, ListingFlags::SortByTime) || has_flag(flags_, ListingFlags::UseAccessTime) || has_flag(flags_, ListingFlags::UseChangeTime) || has_flag(flags_, ListingFlags::ShowBlocks) || has_flag(flags_, ListingFlags::ShowInodes); }

    void stat_file(FileInfo& file_info) {
        const std::filesystem::path p = file_info.name();
        xinim::fs::operation_context ctx;
        ctx.follow_symlinks = true; // Default for ls, unless -l or -d for a symlink

        std::error_code ec_is_symlink_check;
        bool is_symlink_on_path = std::filesystem::is_symlink(std::filesystem::symlink_status(p, ec_is_symlink_check));

        if (is_symlink_on_path) {
            if (has_flag(flags_, ListingFlags::LongFormat) || has_flag(flags_, ListingFlags::DirectoryOnly)) {
                ctx.follow_symlinks = false; // For `ls -l symlink` or `ls -d symlink`, show info about the symlink itself.
            }
        }
        // If DirectoryOnly (-d) is set, and it's a directory (not a symlink to one), follow_symlinks remains true.
        // This is fine as get_status on a directory path doesn't "follow" in a way that changes the path.

        auto result = xinim::fs::get_status(p, ctx);
        if (result) {
            file_info.update_from_xinim_status(result.value());
        } else {
            // Populate with defaults, mark as not performed
            file_info.update_from_xinim_status({});
            throw std::filesystem::filesystem_error(
                std::format("cannot access '{}'", file_info.name()), p, result.error());
        }
    }

    void expand_directory_impl(const std::string& directory_path) {
        try {
            std::size_t file_count_in_dir = 0;
            for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
                if (files_.size() + file_count_in_dir >= constants::MAX_FILES) {
                     throw std::runtime_error("Too many files to process in directory listing");
                }
                const auto filename = entry.path().filename().string();
                if (!has_flag(flags_, ListingFlags::ShowAll) && filename.starts_with('.')) {
                    continue;
                }

                files_.emplace_back(filename); // Use relative filename for listing within directory
                FileInfo& new_file_info = files_.back();
                // Store full path for statting, but keep relative name in FileInfo for display
                const std::filesystem::path full_entry_path = entry.path();

                if (should_stat_file()) {
                    xinim::fs::operation_context entry_ctx;
                    // For entries within a directory:
                    // If -l (LongFormat), we want info about symlink itself, not target.
                    // Otherwise (not -l), we effectively "follow" it by statting the target if it's a directory entry.
                    // std::filesystem::directory_iterator does not follow symlinks itself.
                    // entry.status() or entry.symlink_status() could be used if not for xinim::fs.
                    // Here, we check if the entry is a symlink. If so, and -l, don't follow.
                    entry_ctx.follow_symlinks = true; // Default
                    if (has_flag(flags_, ListingFlags::LongFormat) && entry.is_symlink()) {
                         entry_ctx.follow_symlinks = false;
                    }

                    auto status_result = xinim::fs::get_status(full_entry_path, entry_ctx);
                    if (status_result) {
                        new_file_info.update_from_xinim_status(status_result.value());
                    } else {
                        // Only print error if it's not a hidden file or if hidden files are shown
                        if (!filename.starts_with('.') || has_flag(flags_, ListingFlags::ShowAll)) {
                             std::println(std::cerr, "ls: cannot access '{}': {}", full_entry_path.string(), status_result.error().message());
                        }
                        overall_exit_status_ = 1; // EXIT_FAILURE
                        new_file_info.update_from_xinim_status({}); // Populate with defaults
                    }
                }
                file_count_in_dir++;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::println(std::cerr, "ls: cannot read directory '{}': {} ({})", directory_path, e.what(), e.code().message());
            overall_exit_status_ = 1; // EXIT_FAILURE
        }
        // POSIX fallback removed as it's not fully updated and std::filesystem should be preferred.
    }

    // read_directory_posix is removed as it's a fallback and not fully aligned with xinim::fs.
    // If std::filesystem::directory_iterator fails, the exception is caught.

    void sort_files() { /* ... same ... */
        if (has_flag(flags_, ListingFlags::NoSort) && files_.size() > 0) { sort_indices_.resize(files_.size()); std::iota(sort_indices_.begin(), sort_indices_.end(), 0); return; }
        if (files_.empty()) { sort_indices_.clear(); return; }
        sort_indices_.resize(files_.size()); std::iota(sort_indices_.begin(), sort_indices_.end(), 0);
        const auto use_parallel = files_.size() > 1000;
        auto get_file_ref = [&](std::size_t index) -> const FileInfo& { return files_[index]; };
        if (has_flag(flags_, ListingFlags::SortByTime)) {
            const auto time_comparator = [&](std::size_t a, std::size_t b) {
                const auto& file_a = get_file_ref(a); const auto& file_b = get_file_ref(b);
                auto time_a = get_sort_time(file_a); auto time_b = get_sort_time(file_b);
                bool result = time_a > time_b;
                if (time_a == time_b) { return simd_ops::compare_strings_simd(file_a.name(), file_b.name()) < 0; }
                return has_flag(flags_, ListingFlags::ReverseSort) ? !result : result;
            };
            if (use_parallel) std::sort(std::execution::par_unseq, sort_indices_.begin(), sort_indices_.end(), time_comparator);
            else std::ranges::sort(sort_indices_, time_comparator);
        } else {
            const auto name_comparator = [&](std::size_t a, std::size_t b) {
                const auto cmp_result = simd_ops::compare_strings_simd(get_file_ref(a).name(), get_file_ref(b).name());
                bool result = cmp_result < 0;
                return has_flag(flags_, ListingFlags::ReverseSort) ? !result : result;
            };
            if (use_parallel) std::sort(std::execution::par_unseq, sort_indices_.begin(), sort_indices_.end(), name_comparator);
            else std::ranges::sort(sort_indices_, name_comparator);
        }
    }

    [[nodiscard]] std::chrono::system_clock::time_point get_sort_time(const FileInfo& file_info) const { /* ... same ... */
        if (has_flag(flags_, ListingFlags::UseAccessTime)) return file_info.access_time();
        if (has_flag(flags_, ListingFlags::UseChangeTime)) return file_info.status_change_time();
        return file_info.modification_time();
    }

    void print_listing() const { /* ... same ... */
        if ((has_flag(flags_, ListingFlags::LongFormat) || has_flag(flags_, ListingFlags::ShowBlocks)) && !files_.empty() && files_.at(0).is_stat_performed()) {
             // Only print total blocks if stats were actually performed for at least one file.
             // And if there are files to list.
            bool any_stat_performed = false;
            for(const auto& index : sort_indices_){ if(files_[index].is_stat_performed()) { any_stat_performed = true; break; } }
            if(any_stat_performed) print_total_blocks();
        }
        for (const auto& index : sort_indices_) { print_file_line(files_[index]); }
    }

    void print_total_blocks() const { /* ... same ... */
        if (!has_flag(flags_, ListingFlags::LongFormat) && !has_flag(flags_, ListingFlags::ShowBlocks)) return;
        const auto total_blocks = std::accumulate(sort_indices_.begin(), sort_indices_.end(), 0ULL,
            [&](std::uintmax_t sum, std::size_t index) {
                const auto& file_info = files_[index];
                // Only count blocks for files/dirs that were successfully stat'd.
                // For -d on a directory, its own size might be listed as blocks.
                // Standard ls usually shows 0 for directories unless -s on directory argument.
                // This logic is simplified: if stat'd, and not a directory OR -d is set, count its blocks.
                if (file_info.is_stat_performed() && (!file_info.is_directory() || has_flag(flags_, ListingFlags::DirectoryOnly)) ) {
                     return sum + calculate_blocks(file_info.size());
                }
                return sum;
            });
        std::println(std::cout, "total {}", total_blocks);
    }

    void print_file_line(const FileInfo& file_info) const { /* ... same ... */
        std::string line_buffer;
        if (has_flag(flags_, ListingFlags::ShowInodes)) { line_buffer += std::format("{:5} ", file_info.is_stat_performed() ? std::to_string(file_info.inode()) : "?"s); }
        if (has_flag(flags_, ListingFlags::ShowBlocks)) { line_buffer += std::format("{:4} ", file_info.is_stat_performed() ? std::to_string(calculate_blocks(file_info.size())) : "?"s); }
        if (has_flag(flags_, ListingFlags::LongFormat)) {
            if (file_info.is_stat_performed()) { line_buffer += get_long_format_string(file_info); }
            else { line_buffer += "-????????? ? ?        ?              ? "; } // Placeholder for files that couldn't be stat'd
        }
        line_buffer += file_info.name();
        // TODO: Handle symlink target display for -l: "symlink_name -> target"
        if (has_flag(flags_, ListingFlags::LongFormat) && S_ISLNK(file_info.mode()) && file_info.is_stat_performed()) {
            xinim::fs::operation_context readlink_ctx; // Default context is fine
            readlink_ctx.follow_symlinks = false; // read_symlink inherently doesn't follow
            auto target_path_result = xinim::fs::read_symlink(file_info.name(), readlink_ctx); // Use original name if it's a path
            if (target_path_result) {
                line_buffer += " -> " + target_path_result.value().string();
            } else {
                line_buffer += " -> [error reading link]";
            }
        }
        std::println(std::cout, "{}", line_buffer);
    }

    [[nodiscard]] std::string get_long_format_string(const FileInfo& file_info) const { /* ... same ... */
        std::string part;
        part.reserve(60); // Pre-allocate
        part += std::format("{} {:2} ", PermissionFormatter::format_permissions(file_info.mode()), file_info.link_count());
        if (has_flag(flags_, ListingFlags::ShowGroup)) {
            if (auto group_name = user_cache_.get_groupname(file_info.gid())) part += std::format("{:<8} ", *group_name);
            else part += std::format("{:<8} ", file_info.gid());
        } else {
            if (auto user_name = user_cache_.get_username(file_info.uid())) part += std::format("{:<8} ", *user_name);
            else part += std::format("{:<8} ", file_info.uid());
        }
        if (file_info.is_device()) part += std::format("{:3}, {:3} ", (file_info.size() >> 8) & 0xFF, file_info.size() & 0xFF); // Major/minor from rdevice stored in size for devices
        else part += std::format("{:8} ", file_info.size());
        part += get_formatted_time_string(file_info.modification_time());
        return part;
    }

    [[nodiscard]] std::string get_formatted_time_string(const std::chrono::system_clock::time_point& time) const { /* ... same ... */
        const auto time_t_val = std::chrono::system_clock::to_time_t(time);
        // Using thread-safe localtime_r or gmtime_r is preferred in multi-threaded contexts,
        // but for ls, which is typically single-threaded per invocation, std::localtime is common.
        // For robust C++20/23, std::chrono::format could be used if available and suitable.
        const auto* tm_info = std::localtime(&time_t_val);
        static constexpr std::array month_names{ "Jan"sv, "Feb"sv, "Mar"sv, "Apr"sv, "May"sv, "Jun"sv, "Jul"sv, "Aug"sv, "Sep"sv, "Oct"sv, "Nov"sv, "Dec"sv };
        std::string time_str; time_str.reserve(14);
        if (tm_info) {
            const auto current_time_t_val = std::chrono::system_clock::to_time_t(current_time_);
            const double age_seconds = std::difftime(current_time_t_val, time_t_val);
            time_str += month_names[tm_info->tm_mon];
            time_str += std::format(" {:2} ", tm_info->tm_mday);
            if (std::abs(age_seconds) > (constants::SECONDS_PER_YEAR / 2.0)) { // Older than ~6 months
                time_str += std::format(" {:4}", tm_info->tm_year + 1900); // Show year
            } else {
                time_str += std::format("{:02}:{:02}", tm_info->tm_hour, tm_info->tm_min); // Show HH:MM
            }
        } else { time_str = "             "; } // Fallback if time conversion fails
        return time_str;
    }

    [[nodiscard]] static constexpr std::uintmax_t calculate_blocks(FileInfo::FileSize size) noexcept { /* ... same ... */
        constexpr std::uintmax_t FS_BLOCK_SIZE = 512; // This is a common convention for blocks in `ls`
        return (size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    }
};

} // namespace xinim::commands::ls

/**
 * @brief Entry point for the ls utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char* argv[]) { /* ... same ... */
    // Set locale to user's preference to ensure correct time formatting, character sets, etc.
    // std::locale::global(std::locale("")); // Potentially controversial, might affect other parts if XINIM is a library
    // For a standalone command, this is more common.
    // However, std::println and std::format are locale-independent by default.
    // Time formatting via std::localtime is locale-dependent.

    using namespace xinim::commands::ls;
    // Consider EXIT_SUCCESS and EXIT_FAILURE from <cstdlib>
    // int exit_status_success = 0; // Typically EXIT_SUCCESS
    // int exit_status_minor_problems = 1; // Typically EXIT_FAILURE for some tools
    // int exit_status_serious_trouble = 2; // Typically EXIT_FAILURE or distinct code for others

    try {
        DirectoryLister lister{static_cast<ListingFlags>(0)};
        return lister.process_arguments(argc, argv);
    } catch (const std::exception& e) {
        // This catch block is for truly unexpected errors bubbling up from DirectoryLister constructor or initial setup.
        std::println(std::cerr, "ls: a critical unexpected error occurred: {}", e.what());
        return 2; // Or EXIT_FAILURE
    } catch (...) {
        std::println(std::cerr, "ls: an unknown fatal error occurred.");
        return 2; // Or EXIT_FAILURE
    }
}

// include/xinim/filesystem.hpp
#pragma once

#include <filesystem>
#include <expected>
#include <system_error> // For std::error_code
#include <string_view>
#include <optional>     // For std::optional in set_file_times
#include <chrono>       // For std::chrono types
#include <cstddef>      // For std::byte, std::size_t
#include <span>         // For std::span (C++20)

// POSIX types for function signatures where std::filesystem types are not used directly
// These are typically included via <sys/types.h> and <unistd.h>
// For cross-platform header parsing, ensure they are defined or aliased.
#ifndef _WIN32
    #include <sys/types.h> // For mode_t, uid_t, gid_t, dev_t, ino_t, nlink_t
    #include <unistd.h>    // For uid_t, gid_t (sometimes) and ::close
#else
    // Placeholder types for Windows if strictly needed for compilation
    // XINIM will have its own consistent type definitions.
    // These need to be actual type aliases, not just 'using' for forward declarations.
    typedef unsigned short mode_t_placeholder; // Using a placeholder suffix to avoid conflict if mode_t is already defined somehow
    typedef int uid_t_placeholder;
    typedef int gid_t_placeholder;
    typedef int dev_t_placeholder;
    typedef unsigned long long ino_t_placeholder;
    typedef unsigned short nlink_t_placeholder;

    // Alias them for use in this header
    using mode_t = mode_t_placeholder;
    using uid_t = uid_t_placeholder;
    using gid_t = gid_t_placeholder;
    using dev_t = dev_t_placeholder;
    using ino_t = ino_t_placeholder;
    using nlink_t = nlink_t_placeholder;
#endif

namespace xinim::fs {

    enum class mode {
        standard,    // Prefer std::filesystem or higher-level abstractions
        direct,      // Prefer direct POSIX/XINIM VFS calls
        auto_detect  // Layer decides based on context (e.g., environment, path type)
    };

    struct operation_context {
        mode execution_mode {mode::auto_detect};
        bool audit_enabled {false};
        // For operations that might dereference a symlink (like stat, chown, chmod, set_times),
        // this flag indicates if the operation should apply to the symlink target or the link itself.
        // Default to true (like stat, chown default) vs. false (like lstat, lchown default).
        bool follow_symlinks {true};

        // Other context flags can be added, e.g., umask for creation operations.
        operation_context() = default;
    };

    // --- File Status Structure ---
    struct file_status_ex {
        std::filesystem::file_type type {std::filesystem::file_type::none};
        std::filesystem::perms permissions {std::filesystem::perms::unknown};
        ::uid_t uid {static_cast< ::uid_t>(-1)}; // Use global scope ::uid_t
        ::gid_t gid {static_cast< ::gid_t>(-1)}; // Use global scope ::gid_t
        std::uintmax_t file_size {0};
        ::nlink_t link_count {0}; // Use global scope ::nlink_t
        ::dev_t device {0};       // Use global scope ::dev_t
        ::ino_t inode {0};        // Use global scope ::ino_t
        ::dev_t rdevice {0};      // Use global scope ::dev_t

        std::chrono::system_clock::time_point mtime{};
        std::chrono::system_clock::time_point atime{};
        std::chrono::system_clock::time_point ctime{};

        bool is_populated {false};
        file_status_ex() = default;
    };

    // --- Free Functions for Filesystem Operations ---

    // Status
    std::expected<file_status_ex, std::error_code> get_status(
        const std::filesystem::path& path,
        const operation_context& ctx = {});

    // Directory Creation
    std::expected<void, std::error_code> create_directory(
        const std::filesystem::path& path,
        std::filesystem::perms prms = std::filesystem::perms::all, // Default perms for new dir
        const operation_context& ctx = {});

    std::expected<void, std::error_code> create_directories( // Renamed from create_directories_hybrid
        const std::filesystem::path& path,
        std::filesystem::perms prms_for_final_dir = std::filesystem::perms::all,
        const operation_context& ctx = {});

    // Permissions
    std::expected<void, std::error_code> change_permissions(
        const std::filesystem::path& path,
        std::filesystem::perms perms, // Caller constructs the final desired permission set
        const operation_context& ctx = {});

    // Ownership
    std::expected<void, std::error_code> change_ownership(
        const std::filesystem::path& path,
        ::uid_t uid, // Use global scope ::uid_t
        ::gid_t gid, // Use global scope ::gid_t
        const operation_context& ctx = {});

    // Removal
    std::expected<void, std::error_code> remove( // Renamed from remove_hybrid
        const std::filesystem::path& path,
        const operation_context& ctx = {});

    std::expected<std::uintmax_t, std::error_code> remove_all( // Renamed from remove_all_hybrid
        const std::filesystem::path& path,
        const operation_context& ctx = {});

    // Links
    std::expected<void, std::error_code> create_symlink( // Renamed from create_symlink_hybrid
        const std::filesystem::path& target,
        const std::filesystem::path& link,
        const operation_context& ctx = {});

    std::expected<void, std::error_code> create_hard_link( // Renamed from create_hard_link_hybrid
        const std::filesystem::path& target,
        const std::filesystem::path& link,
        const operation_context& ctx = {});

    std::expected<std::filesystem::path, std::error_code> read_symlink( // Renamed from read_symlink_hybrid
        const std::filesystem::path& path,
        const operation_context& ctx = {});

    // Timestamps
    std::expected<void, std::error_code> set_file_times( // Renamed from set_file_times_hybrid
        const std::filesystem::path& path,
        const std::optional<std::filesystem::file_time_type>& access_time,
        const std::optional<std::filesystem::file_time_type>& modification_time,
        const operation_context& ctx = {});

    // File Creation
    std::expected<void, std::error_code> create_file( // Renamed from create_file_hybrid
        const std::filesystem::path& path,
        std::filesystem::perms prms = static_cast<std::filesystem::perms>(0666), // Default before umask
        bool fail_if_exists = false,
        const operation_context& ctx = {});

    // Rename
    std::expected<void, std::error_code> rename( // Renamed from rename_hybrid
        const std::filesystem::path& old_path,
        const std::filesystem::path& new_path,
        const operation_context& ctx = {});

    // File Copy (helpers and main)
    std::expected<void, std::error_code> copy_file(
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        std::filesystem::copy_options options,
        const operation_context& ctx = {});

    std::expected<void, std::error_code> copy_symlink(
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        const operation_context& ctx = {});

    std::expected<void, std::error_code> copy(
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        std::filesystem::copy_options options,
        const operation_context& ctx = {});


    // Extended Attributes
    std::expected<void, std::error_code> set_extended_attr(
        const std::filesystem::path& path,
        std::string_view name,
        std::span<const std::byte> value,
        const operation_context& ctx = {});

    // Other xattr functions like get_extended_attr, list_extended_attrs, remove_extended_attr would go here.

    // Filesystem Statistics
    // struct filesystem_stats_ex;
    // std::expected<filesystem_stats_ex, std::error_code> get_filesystem_stats(
    //    const std::filesystem::path& path,
    //    const operation_context& ctx = {});


    // --- RAII File Descriptor Wrapper ---
    class file_descriptor {
        int fd_ = -1;
    public:
        explicit file_descriptor(int fd = -1) noexcept : fd_(fd) {}
        ~file_descriptor() {
            if (fd_ >= 0) {
                ::close(fd_); // Ensure ::close is from unistd.h
            }
        }

        file_descriptor(const file_descriptor&) = delete;
        file_descriptor& operator=(const file_descriptor&) = delete;

        file_descriptor(file_descriptor&& other) noexcept : fd_(other.fd_) {
            other.fd_ = -1;
        }

        file_descriptor& operator=(file_descriptor&& other) noexcept {
            if (this != &other) {
                if (fd_ >= 0) {
                    ::close(fd_);
                }
                fd_ = other.fd_;
                other.fd_ = -1;
            }
            return *this;
        }

        [[nodiscard]] int get() const noexcept { return fd_; }
        [[nodiscard]] int release() noexcept { int tmp = fd_; fd_ = -1; return tmp; }
        [[nodiscard]] bool is_valid() const noexcept { return fd_ >= 0; }

        // Example safe read/write using span (conceptual, implementation in .cpp)
        // std::expected<ssize_t, std::error_code> read(std::span<std::byte> buffer) noexcept;
        // std::expected<ssize_t, std::error_code> write(std::span<const std::byte> buffer) noexcept;
    };

} // namespace xinim::fs

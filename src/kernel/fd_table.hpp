/**
 * @file fd_table.hpp
 * @brief File descriptor table management
 *
 * Provides per-process file descriptor table for managing open files.
 * Each process has its own FD table with up to MAX_FDS_PER_PROCESS entries.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 */

#ifndef XINIM_KERNEL_FD_TABLE_HPP
#define XINIM_KERNEL_FD_TABLE_HPP

#include <cstddef>
#include <cstdint>

namespace xinim::kernel {

    // ============================================================================
    // File Descriptor Limits
    // ============================================================================

    /**
     * @brief Maximum file descriptors per process
     *
     * The process control block embeds this fixed-capacity table. Runtime
     * descriptor limits may select a smaller usable prefix.
     */
    constexpr std::size_t MAX_FDS_PER_PROCESS = 1024U;

    /**
     * @brief Standard file descriptor numbers
     */
    constexpr int STDIN_FILENO = 0;  ///< Standard input
    constexpr int STDOUT_FILENO = 1; ///< Standard output
    constexpr int STDERR_FILENO = 2; ///< Standard error

    // ============================================================================
    // File Descriptor Flags
    // ============================================================================

    /**
     * @brief File descriptor flags (from fcntl F_GETFD/F_SETFD)
     */
    enum class FdFlags : std::uint32_t {
        NONE = 0,
        CLOEXEC = (1U << 0U), ///< Close on exec (FD_CLOEXEC)
    };

    /**
     * @brief File status flags (from open() flags parameter)
     *
     * Matches POSIX open() flags for compatibility.
     */
    enum class FileFlags : std::uint32_t {
        // Access mode (mutually exclusive bits 0-1)
        RDONLY = 0x0000,  ///< Read only (O_RDONLY)
        WRONLY = 0x0001,  ///< Write only (O_WRONLY)
        RDWR = 0x0002,    ///< Read/write (O_RDWR)
        ACCMODE = 0x0003, ///< Mask for access mode

        // Creation and file status flags
        CREAT = 0x0040,      ///< Create if doesn't exist (O_CREAT)
        EXCL = 0x0080,       ///< Exclusive open (O_EXCL)
        NOCTTY = 0x0100,     ///< Don't make controlling TTY (O_NOCTTY)
        TRUNC = 0x0200,      ///< Truncate (O_TRUNC)
        APPEND = 0x0400,     ///< Append mode (O_APPEND)
        NONBLOCK = 0x0800,   ///< Non-blocking I/O (O_NONBLOCK)
        DSYNC = 0x1000,      ///< Synchronous I/O for data (O_DSYNC)
        DIRECT = 0x4000,     ///< Direct I/O (O_DIRECT)
        LARGEFILE = 0x8000,  ///< Large file support (O_LARGEFILE)
        DIRECTORY = 0x10000, ///< Must be directory (O_DIRECTORY)
        NOFOLLOW = 0x20000,  ///< Don't follow symlinks (O_NOFOLLOW)
        CLOEXEC = 0x80000,   ///< Close on exec (O_CLOEXEC)
        SYNC = 0x101000,     ///< Synchronous I/O (O_SYNC)
    };

    // ============================================================================
    // File Descriptor Entry
    // ============================================================================

    /**
     * @brief Single file descriptor entry
     *
     * Represents one open file in a process's FD table.
     */
    struct FileDescriptor {
        bool is_open;             ///< Is this FD allocated?
        std::uint32_t flags;      ///< FD flags (FdFlags)
        std::uint32_t file_flags; ///< File status flags (FileFlags)
        std::uint64_t offset;     ///< Current file position
        void *inode;              ///< Pointer to VFS inode (or device-specific structure)
        void *private_data;       ///< Driver/filesystem-specific data

        /**
         * @brief Initialize as closed FD
         */
        void reset() noexcept {
            is_open = false;
            flags = 0U;
            file_flags = 0U;
            offset = 0U;
            inode = nullptr;
            private_data = nullptr;
        }
    };

    // ============================================================================
    // File Descriptor Table
    // ============================================================================

    /**
     * @brief File descriptor table (per-process)
     *
     * Each process has one FD table containing all its open files.
     * FD 0-2 are reserved for stdin/stdout/stderr.
     */
    struct FileDescriptorTable {
        FileDescriptor fds[MAX_FDS_PER_PROCESS]; ///< Array of FD entries
        std::uint32_t next_fd;                   ///< Hint for next free FD (optimization)

        /**
         * @brief Initialize empty FD table
         *
         * All FDs are marked as closed.
         */
        void initialize() noexcept;

        /**
         * @brief Allocate a new file descriptor
         *
         * Finds lowest available FD number and marks it as allocated.
         *
         * @return FD number (>= 0), or -EMFILE if table is full
         */
        int allocate_fd() noexcept;

        /**
         * @brief Allocate specific FD number
         *
         * Used for dup2() to allocate a specific FD.
         *
         * @param descriptor Desired descriptor number
         * @return descriptor on success, or -EBADF when out of range
         */
        int allocate_specific_fd(int descriptor) noexcept;

        /**
         * @brief Get file descriptor entry
         *
         * @param descriptor File descriptor number
         * @return Pointer to FD entry, or nullptr if invalid FD
         */
        FileDescriptor *get_fd(int descriptor) noexcept;

        /**
         * @brief Get file descriptor entry (const version)
         *
         * @param descriptor File descriptor number
         * @return Pointer to FD entry, or nullptr if invalid FD
         */
        const FileDescriptor *get_fd(int descriptor) const noexcept;

        /**
         * @brief Check if FD is valid and open
         *
         * @param descriptor File descriptor number
         * @return true if FD is valid and open, false otherwise
         */
        bool is_valid_fd(int descriptor) const noexcept;

        /**
         * @brief Close and deallocate file descriptor
         *
         * Marks FD as closed and clears all fields.
         * Does NOT call VFS close or release inode (caller must do this).
         *
         * @param descriptor File descriptor number
         * @return 0 on success, -EBADF if invalid FD
         */
        int close_fd(int descriptor) noexcept;

        /**
         * @brief Duplicate file descriptor
         *
         * Creates a copy of source_descriptor pointing to the same backend.
         * A nonnegative target_descriptor selects a specific slot; a negative
         * target_descriptor selects the lowest available slot.
         *
         * @param source_descriptor Source descriptor to duplicate
         * @param target_descriptor Destination descriptor, or -1 for any
         * @return New FD number on success, negative error code on failure
         *         -EBADF if source_descriptor is invalid
         *         -EMFILE if no descriptors are available
         */
        int dup_fd(int source_descriptor, int target_descriptor) noexcept;

        /**
         * @brief Close all FDs marked with CLOEXEC flag
         *
         * Called during exec() to close FDs that should not be inherited.
         */
        void close_on_exec() noexcept;

        /**
         * @brief Get count of open file descriptors
         *
         * @return Number of open FDs
         */
        std::size_t count_open_fds() const noexcept;

        /**
         * @brief Clone FD table for fork()
         *
         * Copies descriptor slots into a child table. The backend owner must
         * acquire shared object references after this structural copy.
         *
         * @param destination Destination FD table (child process)
         * @return 0 on success, negative error on failure
         */
        int clone_to(FileDescriptorTable *destination) const noexcept;
    };

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_FD_TABLE_HPP */

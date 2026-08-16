/**
 * @file fd_table.cpp
 * @brief File descriptor table management implementation
 *
 * @ingroup kernel
 * @author XINIM Development Team
 */

#include "fd_table.hpp"

#include <cerrno>

namespace xinim::kernel {

    // ============================================================================
    // File Descriptor Table Implementation
    // ============================================================================

    /**
     * @brief Initialize empty FD table
     *
     * All FDs are marked as closed. next_fd hint is set to 0.
     */
    void FileDescriptorTable::initialize() noexcept {
        for (std::size_t descriptor_index = 0; descriptor_index < MAX_FDS_PER_PROCESS;
             ++descriptor_index) {
            fds[descriptor_index].reset();
        }
        next_fd = 0U;
    }

    /**
     * @brief Allocate a new file descriptor
     *
     * Uses next_fd as a hint to start searching for free FD.
     * Guarantees to return lowest available FD number.
     *
     * @return FD number (>= 0), or -EMFILE if table is full
     */
    int FileDescriptorTable::allocate_fd() noexcept {
        for (std::uint32_t descriptor_index = next_fd; descriptor_index < MAX_FDS_PER_PROCESS;
             ++descriptor_index) {
            if (!fds[descriptor_index].is_open) {
                fds[descriptor_index].reset();
                fds[descriptor_index].is_open = true;
                next_fd = descriptor_index + 1U;
                return static_cast<int>(descriptor_index);
            }
        }

        for (std::uint32_t descriptor_index = 0U; descriptor_index < next_fd; ++descriptor_index) {
            if (!fds[descriptor_index].is_open) {
                fds[descriptor_index].reset();
                fds[descriptor_index].is_open = true;
                next_fd = descriptor_index + 1U;
                return static_cast<int>(descriptor_index);
            }
        }

        return -EMFILE;
    }

    /**
     * @brief Allocate specific FD number
     *
     * Used by dup2() to allocate a specific FD, closing it first if needed.
     *
     * @param fd Desired FD number
     * @return fd on success, or -EBADF if fd is out of range
     */
    int FileDescriptorTable::allocate_specific_fd(int descriptor) noexcept {
        if (descriptor < 0 || descriptor >= static_cast<int>(MAX_FDS_PER_PROCESS)) {
            return -EBADF;
        }

        if (fds[descriptor].is_open) {
            static_cast<void>(close_fd(descriptor));
        }

        fds[descriptor].reset();
        fds[descriptor].is_open = true;

        return descriptor;
    }

    /**
     * @brief Get file descriptor entry
     *
     * @param fd File descriptor number
     * @return Pointer to FD entry, or nullptr if invalid
     */
    FileDescriptor *FileDescriptorTable::get_fd(int descriptor) noexcept {
        if (descriptor < 0 || descriptor >= static_cast<int>(MAX_FDS_PER_PROCESS)) {
            return nullptr;
        }

        if (!fds[descriptor].is_open) {
            return nullptr;
        }

        return &fds[descriptor];
    }

    /**
     * @brief Get file descriptor entry (const version)
     *
     * @param fd File descriptor number
     * @return Pointer to FD entry, or nullptr if invalid
     */
    const FileDescriptor *FileDescriptorTable::get_fd(int descriptor) const noexcept {
        if (descriptor < 0 || descriptor >= static_cast<int>(MAX_FDS_PER_PROCESS)) {
            return nullptr;
        }

        if (!fds[descriptor].is_open) {
            return nullptr;
        }

        return &fds[descriptor];
    }

    /**
     * @brief Check if FD is valid and open
     *
     * @param fd File descriptor number
     * @return true if valid and open, false otherwise
     */
    bool FileDescriptorTable::is_valid_fd(int descriptor) const noexcept {
        if (descriptor < 0 || descriptor >= static_cast<int>(MAX_FDS_PER_PROCESS)) {
            return false;
        }

        return fds[descriptor].is_open;
    }

    /**
     * @brief Close and deallocate file descriptor
     *
     * Marks FD as closed. Does NOT call VFS close (caller must do this).
     *
     * @param fd File descriptor number
     * @return 0 on success, -EBADF if invalid FD
     */
    int FileDescriptorTable::close_fd(int descriptor) noexcept {
        if (descriptor < 0 || descriptor >= static_cast<int>(MAX_FDS_PER_PROCESS)) {
            return -EBADF;
        }

        if (!fds[descriptor].is_open) {
            return -EBADF;
        }

        fds[descriptor].reset();

        const auto descriptor_index = static_cast<std::uint32_t>(descriptor);
        if (descriptor_index < next_fd) {
            next_fd = descriptor_index;
        }

        return 0;
    }

    /**
     * @brief Duplicate file descriptor
     *
     * Creates a copy of oldfd. If newfd >= 0, uses that FD.
     * Otherwise, allocates lowest available FD.
     *
     * @param oldfd Source FD to duplicate
     * @param newfd Destination FD (or -1 for any)
     * @return New FD number on success, negative error on failure
     */
    int FileDescriptorTable::dup_fd(int source_descriptor, int target_descriptor) noexcept {
        if (!is_valid_fd(source_descriptor)) {
            return -EBADF;
        }
        if (source_descriptor == target_descriptor) {
            return source_descriptor;
        }

        int result_descriptor = -1;

        if (target_descriptor < 0) {
            result_descriptor = allocate_fd();
            if (result_descriptor < 0) {
                return result_descriptor;
            }
        } else {
            result_descriptor = allocate_specific_fd(target_descriptor);
            if (result_descriptor < 0) {
                return result_descriptor;
            }
        }

        fds[result_descriptor] = fds[source_descriptor];
        fds[result_descriptor].flags = 0U;

        return result_descriptor;
    }

    /**
     * @brief Close all FDs marked with CLOEXEC flag
     *
     * Called during exec() to close FDs that should not be inherited.
     */
    void FileDescriptorTable::close_on_exec() noexcept {
        for (std::size_t descriptor_index = 0; descriptor_index < MAX_FDS_PER_PROCESS;
             ++descriptor_index) {
            if (fds[descriptor_index].is_open &&
                (fds[descriptor_index].flags & static_cast<std::uint32_t>(FdFlags::CLOEXEC)) !=
                    0U) {
                static_cast<void>(close_fd(static_cast<int>(descriptor_index)));
            }
        }
    }

    /**
     * @brief Get count of open file descriptors
     *
     * @return Number of open FDs
     */
    std::size_t FileDescriptorTable::count_open_fds() const noexcept {
        std::size_t open_count = 0U;
        for (std::size_t descriptor_index = 0; descriptor_index < MAX_FDS_PER_PROCESS;
             ++descriptor_index) {
            if (fds[descriptor_index].is_open) {
                ++open_count;
            }
        }
        return open_count;
    }

    /**
     * @brief Clone FD table for fork()
     *
     * Copies descriptor slots into a child table. The backend owner acquires
     * shared object references after this structural copy.
     *
     * @param dest Destination FD table (child process)
     * @return 0 on success, negative error on failure
     */
    int FileDescriptorTable::clone_to(FileDescriptorTable *destination) const noexcept {
        if (destination == nullptr) {
            return -EINVAL;
        }

        destination->initialize();

        for (std::size_t descriptor_index = 0; descriptor_index < MAX_FDS_PER_PROCESS;
             ++descriptor_index) {
            if (fds[descriptor_index].is_open) {
                destination->fds[descriptor_index] = fds[descriptor_index];
            }
        }

        destination->next_fd = next_fd;

        return 0;
    }

} // namespace xinim::kernel

/**
 * @file fd_table.cpp
 * @brief File descriptor table management implementation
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "fd_table.hpp"
#include <cerrno>
#include <cstring>

namespace xinim::kernel {

// ============================================================================
// File Descriptor Table Implementation
// ============================================================================

/**
 * @brief Initialize empty FD table
 *
 * All FDs are marked as closed. next_fd hint is set to 0.
 */
void FileDescriptorTable::initialize() {
    // Mark all FDs as closed
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; i++) {
        fds[i].reset();
    }

    // Start searching from FD 0
    next_fd = 0;
}

/**
 * @brief Allocate a new file descriptor
 *
 * Uses next_fd as a hint to start searching for free FD.
 * Guarantees to return lowest available FD number.
 *
 * @return FD number (>= 0), or -EMFILE if table is full
 */
int FileDescriptorTable::allocate_fd() {
    // Search from next_fd hint to end
    for (uint32_t i = next_fd; i < MAX_FDS_PER_PROCESS; i++) {
        if (!fds[i].is_open) {
            fds[i].is_open = true;  // Mark as allocated
            fds[i].reset();         // Clear all fields
            fds[i].is_open = true;  // Keep allocated flag set
            next_fd = i + 1;         // Update hint
            return (int)i;
        }
    }

    // Search from 0 to next_fd hint (wrap-around)
    for (uint32_t i = 0; i < next_fd; i++) {
        if (!fds[i].is_open) {
            fds[i].is_open = true;
            fds[i].reset();
            fds[i].is_open = true;
            next_fd = i + 1;
            return (int)i;
        }
    }

    // No free FDs available
    return -EMFILE;  // Too many open files
}

/**
 * @brief Allocate specific FD number
 *
 * Used by dup2() to allocate a specific FD, closing it first if needed.
 *
 * @param fd Desired FD number
 * @return fd on success, or -EBADF if fd is out of range
 */
int FileDescriptorTable::allocate_specific_fd(int fd) {
    // Validate FD range
    if (fd < 0 || fd >= (int)MAX_FDS_PER_PROCESS) {
        return -EBADF;
    }

    // Close existing FD if open
    if (fds[fd].is_open) {
        // Note: Caller must call VFS close on inode before this
        close_fd(fd);
    }

    // Allocate FD
    fds[fd].is_open = true;
    fds[fd].reset();
    fds[fd].is_open = true;

    return fd;
}

/**
 * @brief Get file descriptor entry
 *
 * @param fd File descriptor number
 * @return Pointer to FD entry, or nullptr if invalid
 */
FileDescriptor* FileDescriptorTable::get_fd(int fd) {
    if (fd < 0 || fd >= (int)MAX_FDS_PER_PROCESS) {
        return nullptr;
    }

    if (!fds[fd].is_open) {
        return nullptr;
    }

    return &fds[fd];
}

/**
 * @brief Get file descriptor entry (const version)
 *
 * @param fd File descriptor number
 * @return Pointer to FD entry, or nullptr if invalid
 */
const FileDescriptor* FileDescriptorTable::get_fd(int fd) const {
    if (fd < 0 || fd >= (int)MAX_FDS_PER_PROCESS) {
        return nullptr;
    }

    if (!fds[fd].is_open) {
        return nullptr;
    }

    return &fds[fd];
}

/**
 * @brief Check if FD is valid and open
 *
 * @param fd File descriptor number
 * @return true if valid and open, false otherwise
 */
bool FileDescriptorTable::is_valid_fd(int fd) const {
    if (fd < 0 || fd >= (int)MAX_FDS_PER_PROCESS) {
        return false;
    }

    return fds[fd].is_open;
}

/**
 * @brief Close and deallocate file descriptor
 *
 * Marks FD as closed. Does NOT call VFS close (caller must do this).
 *
 * @param fd File descriptor number
 * @return 0 on success, -EBADF if invalid FD
 */
int FileDescriptorTable::close_fd(int fd) {
    // Validate FD
    if (fd < 0 || fd >= (int)MAX_FDS_PER_PROCESS) {
        return -EBADF;
    }

    if (!fds[fd].is_open) {
        return -EBADF;
    }

    // Reset FD entry
    fds[fd].reset();

    // Update next_fd hint if we closed an earlier FD
    if ((uint32_t)fd < next_fd) {
        next_fd = (uint32_t)fd;
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
int FileDescriptorTable::dup_fd(int oldfd, int newfd) {
    // Validate source FD
    if (!is_valid_fd(oldfd)) {
        return -EBADF;
    }

    int result_fd;

    if (newfd < 0) {
        // Allocate any available FD
        result_fd = allocate_fd();
        if (result_fd < 0) {
            return result_fd;  // -EMFILE
        }
    } else {
        // Use specific FD
        result_fd = allocate_specific_fd(newfd);
        if (result_fd < 0) {
            return result_fd;  // -EBADF
        }
    }

    // Copy FD entry (shallow copy)
    fds[result_fd] = fds[oldfd];

    // Keep FD flags separate (don't copy CLOEXEC for dup)
    fds[result_fd].flags = 0;

    return result_fd;
}

/**
 * @brief Close all FDs marked with CLOEXEC flag
 *
 * Called during exec() to close FDs that should not be inherited.
 */
void FileDescriptorTable::close_on_exec() {
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; i++) {
        if (fds[i].is_open) {
            if (fds[i].flags & (uint32_t)FdFlags::CLOEXEC) {
                // TODO Week 9 Phase 2: Call VFS close on inode
                close_fd((int)i);
            }
        }
    }
}

/**
 * @brief Get count of open file descriptors
 *
 * @return Number of open FDs
 */
size_t FileDescriptorTable::count_open_fds() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; i++) {
        if (fds[i].is_open) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Clone FD table for fork()
 *
 * Creates duplicate FD table for child process.
 * All FDs are duplicated (same inodes, but independent offsets).
 *
 * @param dest Destination FD table (child process)
 * @return 0 on success, negative error on failure
 */
int FileDescriptorTable::clone_to(FileDescriptorTable* dest) const {
    if (!dest) {
        return -EINVAL;
    }

    // Initialize destination table
    dest->initialize();

    // Copy all open FDs
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; i++) {
        if (fds[i].is_open) {
            // Shallow copy FD entry
            dest->fds[i] = fds[i];

            // TODO Week 9 Phase 2: Increment inode reference count
            // For now, both parent and child share inode pointer
        }
    }

    dest->next_fd = next_fd;

    return 0;
}

} // namespace xinim::kernel

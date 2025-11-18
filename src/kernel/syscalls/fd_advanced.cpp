/**
 * @file fd_advanced.cpp
 * @brief Advanced file descriptor syscalls (dup, dup2, pipe, fcntl)
 *
 * Implements POSIX file descriptor manipulation for Week 9 Phase 3.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "../syscall_table.hpp"
#include "../pcb.hpp"
#include "../scheduler.hpp"
#include "../uaccess.hpp"
#include "../fd_table.hpp"
#include "../pipe.hpp"
#include "../../early/serial_16550.hpp"
#include <cerrno>
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// sys_dup - Duplicate file descriptor
// ============================================================================

/**
 * @brief Duplicate file descriptor to lowest available FD
 *
 * POSIX: int dup(int oldfd)
 *
 * Creates a copy of oldfd using the lowest available FD number.
 * Both FDs share the same file description (offset, flags).
 *
 * @param oldfd File descriptor to duplicate
 * @return New FD number on success, negative error on failure
 *         -EBADF: oldfd is invalid
 *         -EMFILE: Too many open files
 */
extern "C" int64_t sys_dup(uint64_t oldfd, uint64_t, uint64_t,
                            uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate oldfd
    if (oldfd >= MAX_FDS_PER_PROCESS) return -EBADF;
    if (!current->fd_table.is_valid_fd((int)oldfd)) return -EBADF;

    // Use FD table's dup_fd with newfd = -1 (any available FD)
    int newfd = current->fd_table.dup_fd((int)oldfd, -1);
    if (newfd < 0) return newfd;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_dup(%lu) = %d\n", oldfd, newfd);
    early_serial.write(log_buf);

    return (int64_t)newfd;
}

// ============================================================================
// sys_dup2 - Duplicate file descriptor to specific FD
// ============================================================================

/**
 * @brief Duplicate file descriptor to specific FD number
 *
 * POSIX: int dup2(int oldfd, int newfd)
 *
 * Duplicates oldfd to newfd. If newfd is open, it's closed first (atomically).
 * If oldfd == newfd, returns newfd without closing.
 *
 * @param oldfd Source file descriptor
 * @param newfd Target file descriptor number
 * @return newfd on success, negative error on failure
 *         -EBADF: oldfd invalid or newfd out of range
 */
extern "C" int64_t sys_dup2(uint64_t oldfd, uint64_t newfd,
                             uint64_t, uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate oldfd
    if (oldfd >= MAX_FDS_PER_PROCESS) return -EBADF;
    if (!current->fd_table.is_valid_fd((int)oldfd)) return -EBADF;

    // Validate newfd range
    if (newfd >= MAX_FDS_PER_PROCESS) return -EBADF;

    // Special case: oldfd == newfd (POSIX requirement)
    // Just return newfd without doing anything
    if ((int)oldfd == (int)newfd) {
        return (int64_t)newfd;
    }

    // Close newfd if it's open (atomic operation)
    if (current->fd_table.is_valid_fd((int)newfd)) {
        current->fd_table.close_fd((int)newfd);
    }

    // Duplicate oldfd to newfd
    int result = current->fd_table.dup_fd((int)oldfd, (int)newfd);
    if (result < 0) return result;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_dup2(%lu, %lu) = %lu\n", oldfd, newfd, newfd);
    early_serial.write(log_buf);

    return (int64_t)newfd;
}

// ============================================================================
// sys_pipe - Create pipe
// ============================================================================

/**
 * @brief Create pipe (two FDs: read and write)
 *
 * POSIX: int pipe(int pipefd[2])
 *
 * Creates a pipe with:
 * - pipefd[0]: read end
 * - pipefd[1]: write end
 *
 * Data written to pipefd[1] can be read from pipefd[0].
 * Pipe buffer size: PIPE_BUF (4096 bytes).
 *
 * @param pipefd_addr User space pointer to int[2] array
 * @return 0 on success, negative error on failure
 *         -EFAULT: Invalid pipefd pointer
 *         -EMFILE: Too many open files
 *         -ENOMEM: Out of memory
 */
extern "C" int64_t sys_pipe(uint64_t pipefd_addr, uint64_t, uint64_t,
                             uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate user pointer
    if (!is_user_address(pipefd_addr, 2 * sizeof(int))) {
        return -EFAULT;
    }

    // Allocate pipe structure
    Pipe* pipe = new Pipe();
    if (!pipe) return -ENOMEM;

    // Initialize pipe
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    pipe->read_end_open = true;
    pipe->write_end_open = true;
    pipe->readers_head = nullptr;
    pipe->writers_head = nullptr;

    // Allocate read FD (pipefd[0])
    int read_fd = current->fd_table.allocate_fd();
    if (read_fd < 0) {
        delete pipe;
        return read_fd;  // -EMFILE
    }

    // Allocate write FD (pipefd[1])
    int write_fd = current->fd_table.allocate_fd();
    if (write_fd < 0) {
        current->fd_table.close_fd(read_fd);
        delete pipe;
        return write_fd;  // -EMFILE
    }

    // Set up read FD
    FileDescriptor* read_fd_entry = current->fd_table.get_fd(read_fd);
    read_fd_entry->is_open = true;
    read_fd_entry->flags = 0;
    read_fd_entry->file_flags = (uint32_t)FileFlags::RDONLY;
    read_fd_entry->offset = 0;
    read_fd_entry->inode = nullptr;  // Pipes don't have inodes
    read_fd_entry->private_data = pipe;  // Store pipe pointer

    // Set up write FD
    FileDescriptor* write_fd_entry = current->fd_table.get_fd(write_fd);
    write_fd_entry->is_open = true;
    write_fd_entry->flags = 0;
    write_fd_entry->file_flags = (uint32_t)FileFlags::WRONLY;
    write_fd_entry->offset = 0;
    write_fd_entry->inode = nullptr;  // Pipes don't have inodes
    write_fd_entry->private_data = pipe;  // Same pipe pointer

    // Copy FD array to user space
    int pipefd[2] = { read_fd, write_fd };
    int ret = copy_to_user(pipefd_addr, pipefd, sizeof(pipefd));
    if (ret < 0) {
        current->fd_table.close_fd(read_fd);
        current->fd_table.close_fd(write_fd);
        delete pipe;
        return ret;
    }

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_pipe() = [%d, %d]\n", read_fd, write_fd);
    early_serial.write(log_buf);

    return 0;  // Success
}

// ============================================================================
// sys_fcntl - File control operations
// ============================================================================

/**
 * @brief File control operations
 *
 * POSIX: int fcntl(int fd, int cmd, ... /* arg */)
 *
 * Week 9 Phase 3: Support basic commands
 * - F_DUPFD (0): Duplicate FD to >= arg
 * - F_GETFD (1): Get FD flags
 * - F_SETFD (2): Set FD flags
 * - F_GETFL (3): Get file status flags
 * - F_SETFL (4): Set file status flags
 *
 * @param fd File descriptor
 * @param cmd Command code
 * @param arg Command argument
 * @return Command-specific result or negative error
 *         -EBADF: Invalid FD
 *         -EINVAL: Invalid command
 */
extern "C" int64_t sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                              uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry) return -EBADF;

    switch (cmd) {
        case 0:  // F_DUPFD - Duplicate FD to >= arg
        {
            int newfd = current->fd_table.allocate_fd();
            if (newfd < 0) return newfd;

            // If newfd < arg, keep allocating until we get FD >= arg
            while (newfd < (int)arg && newfd >= 0) {
                current->fd_table.close_fd(newfd);
                newfd = current->fd_table.allocate_fd();
            }

            if (newfd < 0) return -EMFILE;

            int ret = current->fd_table.dup_fd((int)fd, newfd);
            if (ret < 0) return ret;

            return (int64_t)newfd;
        }

        case 1:  // F_GETFD - Get FD flags (FD_CLOEXEC)
            return (int64_t)fd_entry->flags;

        case 2:  // F_SETFD - Set FD flags
            fd_entry->flags = (uint32_t)arg;
            return 0;

        case 3:  // F_GETFL - Get file status flags (O_RDONLY, O_WRONLY, O_NONBLOCK, etc.)
            return (int64_t)fd_entry->file_flags;

        case 4:  // F_SETFL - Set file status flags
            // Note: Can only change O_APPEND, O_NONBLOCK (not access mode)
            // Week 9: Simple implementation, just set flags
            fd_entry->file_flags = (uint32_t)arg;
            return 0;

        default:
            return -EINVAL;  // Invalid command
    }
}

} // namespace xinim::kernel

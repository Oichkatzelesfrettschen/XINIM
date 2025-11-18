/**
 * @file file_ops.cpp
 * @brief File operation syscalls (open, read, write, close, lseek)
 *
 * Implements POSIX file I/O syscalls integrated with VFS and FD tables.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "../syscall_table.hpp"
#include "../uaccess.hpp"
#include "../fd_table.hpp"
#include "../vfs_interface.hpp"
#include "../pcb.hpp"
#include "../scheduler.hpp"
#include "../../early/serial_16550.hpp"
#include <cerrno>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// sys_open - Open file and return file descriptor
// ============================================================================

/**
 * @brief Open file and return file descriptor
 *
 * POSIX: int open(const char *pathname, int flags, mode_t mode)
 *
 * @param pathname_addr User space pointer to path string
 * @param flags File open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.)
 * @param mode File permissions (for O_CREAT)
 * @return FD number on success, negative error code on failure
 *         -EFAULT: Invalid user pointer
 *         -ENAMETOOLONG: Path too long
 *         -ENOENT: File not found (and O_CREAT not set)
 *         -EINVAL: Invalid flags
 *         -EMFILE: Too many open files
 */
extern "C" int64_t sys_open(uint64_t pathname_addr, uint64_t flags, uint64_t mode,
                             uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Copy pathname from user space
    char pathname[PATH_MAX];
    int ret = copy_string_from_user(pathname, pathname_addr, PATH_MAX);
    if (ret < 0) return ret;

    // Validate flags: Access mode must be 0, 1, or 2
    uint32_t access_mode = flags & (uint32_t)FileFlags::ACCMODE;
    if (access_mode > 2) {
        return -EINVAL;
    }

    // Resolve path to inode
    void* inode = vfs_lookup(pathname);
    if (!inode) {
        if (flags & (uint32_t)FileFlags::CREAT) {
            // Try to create new file
            inode = vfs_create(pathname, (uint32_t)mode);
            if (!inode) {
                return -ENOENT;  // Creation failed
            }
        } else {
            return -ENOENT;  // File not found
        }
    }

    // Check if O_EXCL is set with O_CREAT (must fail if file exists)
    if ((flags & (uint32_t)FileFlags::EXCL) && (flags & (uint32_t)FileFlags::CREAT)) {
        // File exists but O_EXCL was set
        return -EEXIST;
    }

    // TODO Week 9 Phase 2: Check permissions based on access mode

    // Allocate file descriptor
    int fd = current->fd_table.allocate_fd();
    if (fd < 0) {
        return fd;  // -EMFILE (too many open files)
    }

    // Set up FD entry
    FileDescriptor* fd_entry = current->fd_table.get_fd(fd);
    if (!fd_entry) {
        // This should never happen
        return -EIO;
    }

    fd_entry->is_open = true;
    fd_entry->flags = 0;

    // Set CLOEXEC if requested
    if (flags & (uint32_t)FileFlags::CLOEXEC) {
        fd_entry->flags |= (uint32_t)FdFlags::CLOEXEC;
    }

    fd_entry->file_flags = (uint32_t)flags;
    fd_entry->offset = 0;
    fd_entry->inode = inode;
    fd_entry->private_data = nullptr;

    // Truncate if requested
    if (flags & (uint32_t)FileFlags::TRUNC) {
        vfs_truncate(inode, 0);
    }

    // Debug logging
    char log_buf[256];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_open(\"%s\", 0x%lx) = %d\n",
                  pathname, flags, fd);
    early_serial.write(log_buf);

    return (int64_t)fd;
}

// ============================================================================
// sys_read - Read from file descriptor
// ============================================================================

/**
 * @brief Read from file descriptor
 *
 * POSIX: ssize_t read(int fd, void *buf, size_t count)
 *
 * @param fd File descriptor
 * @param buf_addr User space buffer address
 * @param count Number of bytes to read
 * @return Number of bytes read (0 = EOF), or negative error code
 *         -EBADF: Invalid FD or FD not open for reading
 *         -EFAULT: Invalid user buffer
 */
extern "C" int64_t sys_read(uint64_t fd, uint64_t buf_addr, uint64_t count,
                             uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry || !fd_entry->is_open) return -EBADF;

    // Check read permission (O_RDONLY or O_RDWR)
    uint32_t access_mode = fd_entry->file_flags & (uint32_t)FileFlags::ACCMODE;
    if (access_mode == (uint32_t)FileFlags::WRONLY) {
        return -EBADF;  // FD not open for reading
    }

    // Validate user buffer
    if (!is_user_address(buf_addr, count)) return -EFAULT;

    // Limit read size (prevent kernel stack overflow)
    if (count > 4096) count = 4096;

    // Read from VFS into kernel buffer
    char kernel_buf[4096];
    ssize_t bytes_read = vfs_read(fd_entry->inode, kernel_buf, count, fd_entry->offset);
    if (bytes_read < 0) {
        return bytes_read;  // VFS error
    }

    // Copy to user space
    if (bytes_read > 0) {
        int ret = copy_to_user(buf_addr, kernel_buf, (size_t)bytes_read);
        if (ret < 0) return ret;
    }

    // Update file offset (unless device)
    if (!vfs_is_device(fd_entry->inode)) {
        fd_entry->offset += (uint64_t)bytes_read;
    }

    return bytes_read;
}

// ============================================================================
// sys_close - Close file descriptor
// ============================================================================

/**
 * @brief Close file descriptor
 *
 * POSIX: int close(int fd)
 *
 * @param fd File descriptor to close
 * @return 0 on success, negative error code on failure
 *         -EBADF: Invalid FD
 */
extern "C" int64_t sys_close(uint64_t fd, uint64_t, uint64_t,
                              uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD range
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;

    // Get FD entry before closing
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry) return -EBADF;

    // TODO Week 9 Phase 2: Call VFS close on inode (decrement ref count)
    // For now, we just deallocate the FD

    // Close FD
    int ret = current->fd_table.close_fd((int)fd);

    // Debug logging
    if (ret == 0) {
        char log_buf[128];
        std::snprintf(log_buf, sizeof(log_buf),
                      "[SYSCALL] sys_close(%lu) = 0\n", fd);
        early_serial.write(log_buf);
    }

    return (int64_t)ret;
}

// ============================================================================
// sys_lseek - Reposition file offset
// ============================================================================

/**
 * @brief Reposition file offset
 *
 * POSIX: off_t lseek(int fd, off_t offset, int whence)
 *
 * @param fd File descriptor
 * @param offset Offset value (interpretation depends on whence)
 * @param whence SEEK_SET (0), SEEK_CUR (1), or SEEK_END (2)
 * @return New file offset on success, negative error code on failure
 *         -EBADF: Invalid FD
 *         -EINVAL: Invalid whence or resulting offset is negative
 *         -ESPIPE: FD refers to a pipe/socket/device
 */
extern "C" int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                              uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry || !fd_entry->is_open) return -EBADF;

    // lseek doesn't work on devices
    if (vfs_is_device(fd_entry->inode)) {
        return -ESPIPE;  // Illegal seek
    }

    // Calculate new offset
    int64_t new_offset;
    switch (whence) {
        case 0:  // SEEK_SET
            new_offset = (int64_t)offset;
            break;

        case 1:  // SEEK_CUR
            new_offset = (int64_t)fd_entry->offset + (int64_t)offset;
            break;

        case 2:  // SEEK_END
            new_offset = (int64_t)vfs_get_size(fd_entry->inode) + (int64_t)offset;
            break;

        default:
            return -EINVAL;  // Invalid whence
    }

    // Validate new offset (must be non-negative)
    if (new_offset < 0) {
        return -EINVAL;
    }

    // Update offset
    fd_entry->offset = (uint64_t)new_offset;

    return new_offset;
}

} // namespace xinim::kernel

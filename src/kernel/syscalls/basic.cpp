/**
 * @file basic.cpp
 * @brief Basic syscall implementations
 *
 * Implements fundamental syscalls: write, getpid, exit
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "../syscall_table.hpp"
#include "../scheduler.hpp"
#include "../pcb.hpp"
#include "../uaccess.hpp"
#include "../fd_table.hpp"
#include "../vfs_interface.hpp"
#include "../early/serial_16550.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// sys_write - Write to file descriptor
// ============================================================================

/**
 * @brief Write to file descriptor
 *
 * Week 9 Phase 1: VFS-integrated implementation using FD table.
 * Week 10+: Will delegate to VFS server via Lattice IPC.
 *
 * @param fd File descriptor
 * @param buf_addr User buffer address
 * @param count Number of bytes to write
 * @return Number of bytes written or negative error code
 *         -EBADF: Invalid FD or FD not open for writing
 *         -EFAULT: Invalid user buffer
 */
int64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t count,
                  uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry || !fd_entry->is_open) return -EBADF;

    // Check write permission (O_WRONLY or O_RDWR)
    uint32_t access_mode = fd_entry->file_flags & (uint32_t)FileFlags::ACCMODE;
    if (access_mode == (uint32_t)FileFlags::RDONLY) {
        return -EBADF;  // FD not open for writing
    }

    // Validate user buffer
    if (!is_user_address(buf_addr, count)) return -EFAULT;

    // Limit write size (prevent kernel stack overflow)
    if (count > 4096) count = 4096;

    // Copy from user space to kernel buffer
    char kernel_buf[4096];
    int ret = copy_from_user(kernel_buf, buf_addr, count);
    if (ret < 0) return ret;

    // Write to VFS
    ssize_t bytes_written = vfs_write(fd_entry->inode, kernel_buf, count, fd_entry->offset);
    if (bytes_written < 0) {
        return bytes_written;  // VFS error
    }

    // Update file offset (always append if APPEND flag set)
    if (fd_entry->file_flags & (uint32_t)FileFlags::APPEND) {
        fd_entry->offset = vfs_get_size(fd_entry->inode);
    } else if (!vfs_is_device(fd_entry->inode)) {
        // Only update offset for regular files, not devices
        fd_entry->offset += (uint64_t)bytes_written;
    }

    return bytes_written;
}

// ============================================================================
// sys_getpid - Get process ID
// ============================================================================

/**
 * @brief Get process ID of calling process
 *
 * @return pid_t (as int64_t)
 */
int64_t sys_getpid(uint64_t, uint64_t, uint64_t,
                   uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();

    if (!current) {
        // Should never happen
        return -1;
    }

    return (int64_t)current->pid;
}

// ============================================================================
// sys_exit - Terminate calling process
// ============================================================================

/**
 * @brief Terminate calling process
 *
 * Week 8 Phase 4: Simple implementation that marks process as ZOMBIE.
 * Future versions will:
 * - Free process resources
 * - Notify parent via wait/waitpid
 * - Handle orphaned children
 *
 * @param status Exit status code
 * @return Does not return
 */
[[noreturn]] int64_t sys_exit(uint64_t status, uint64_t, uint64_t,
                               uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();

    if (!current) {
        // Should never happen, but handle gracefully
        for(;;) { __asm__ volatile ("hlt"); }
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[SYSCALL] Process %d (%s) exiting with status %lu\n",
             current->pid, current->name, status);
    early_serial.write(buffer);

    // Mark process as ZOMBIE
    current->state = ProcessState::ZOMBIE;

    // TODO: Free resources, notify parent, handle children

    // Yield to next process
    schedule();

    // Should never return, but if we do, halt
    for(;;) { __asm__ volatile ("hlt"); }
}

} // namespace xinim::kernel

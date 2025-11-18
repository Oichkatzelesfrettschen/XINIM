/**
 * @file vfs_interface.hpp
 * @brief Kernel-side VFS interface for syscalls
 *
 * Provides minimal VFS functionality for Week 9 Phase 1.
 * Supports /dev/console only. Full VFS integration in Week 10.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_VFS_INTERFACE_HPP
#define XINIM_KERNEL_VFS_INTERFACE_HPP

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

namespace xinim::kernel {

// ============================================================================
// VFS Interface Functions
// ============================================================================

/**
 * @brief Lookup path and return inode
 *
 * Week 9 Phase 1: Only supports /dev/console
 * Week 10: Will support full VFS path resolution
 *
 * @param pathname Absolute path (e.g., "/dev/console")
 * @return Inode pointer, or nullptr if not found
 */
void* vfs_lookup(const char* pathname);

/**
 * @brief Read from inode
 *
 * Reads data from file/device represented by inode.
 *
 * @param inode Inode to read from
 * @param buffer Kernel buffer to read into
 * @param count Number of bytes to read
 * @param offset File offset (ignored for devices)
 * @return Number of bytes read, or negative error code
 *         -EIO if I/O error
 *         -EINVAL if invalid parameters
 */
ssize_t vfs_read(void* inode, void* buffer, size_t count, uint64_t offset);

/**
 * @brief Write to inode
 *
 * Writes data to file/device represented by inode.
 *
 * @param inode Inode to write to
 * @param buffer Kernel buffer to write from
 * @param count Number of bytes to write
 * @param offset File offset (ignored for devices)
 * @return Number of bytes written, or negative error code
 *         -EIO if I/O error
 *         -EINVAL if invalid parameters
 */
ssize_t vfs_write(void* inode, const void* buffer, size_t count, uint64_t offset);

/**
 * @brief Get file/device size
 *
 * Week 9 Phase 1: Returns 0 for devices
 * Week 10: Will return actual file size
 *
 * @param inode Inode to query
 * @return File size in bytes (0 for devices)
 */
uint64_t vfs_get_size(void* inode);

/**
 * @brief Create new file
 *
 * Week 9 Phase 1: Not implemented (returns nullptr)
 * Week 10: Will support file creation
 *
 * @param pathname Path to create
 * @param mode File permissions
 * @return Inode pointer, or nullptr on failure
 */
void* vfs_create(const char* pathname, uint32_t mode);

/**
 * @brief Truncate file to specified size
 *
 * Week 9 Phase 1: Not implemented (returns -ENOSYS)
 * Week 10: Will support truncation
 *
 * @param inode Inode to truncate
 * @param size New size
 * @return 0 on success, negative error on failure
 */
int vfs_truncate(void* inode, uint64_t size);

/**
 * @brief Delete file
 *
 * Week 9 Phase 1: Not implemented (returns -ENOSYS)
 * Week 10: Will support deletion
 *
 * @param pathname Path to delete
 * @return 0 on success, negative error on failure
 */
int vfs_unlink(const char* pathname);

/**
 * @brief Check if inode is a device
 *
 * @param inode Inode to check
 * @return true if device, false if regular file
 */
bool vfs_is_device(void* inode);

/**
 * @brief Check if inode is a directory
 *
 * Week 9 Phase 1: Always returns false
 * Week 10: Will support directories
 *
 * @param inode Inode to check
 * @return true if directory, false otherwise
 */
bool vfs_is_directory(void* inode);

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_VFS_INTERFACE_HPP */

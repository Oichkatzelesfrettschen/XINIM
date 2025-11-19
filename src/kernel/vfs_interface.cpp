/**
 * @file vfs_interface.cpp
 * @brief Minimal VFS implementation for Week 9 Phase 1
 *
 * Provides basic /dev/console support only.
 * Full VFS integration in Week 10.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "vfs_interface.hpp"
#include "../early/serial_16550.hpp"
#include <cstring>
#include <cerrno>

// External early serial console
extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// Minimal Inode Structure (Week 9 Phase 1)
// ============================================================================

/**
 * @brief Simple inode for /dev/console
 *
 * Week 9: Minimal structure for console device
 * Week 10: Will be replaced with full VFS inode
 */
struct SimpleInode {
    const char* path;        ///< Device path
    bool is_device;          ///< Is this a device (vs regular file)?
    bool is_console;         ///< Is this the console device?
};

// Static inode for /dev/console
static SimpleInode g_console_inode = {
    .path = "/dev/console",
    .is_device = true,
    .is_console = true,
};

// ============================================================================
// VFS Lookup
// ============================================================================

/**
 * @brief Lookup path and return inode
 *
 * Week 9 Phase 1: Only recognizes "/dev/console"
 *
 * @param pathname Absolute path
 * @return Inode pointer, or nullptr if not found
 */
void* vfs_lookup(const char* pathname) {
    if (!pathname) {
        return nullptr;
    }

    // Check for /dev/console
    if (std::strcmp(pathname, "/dev/console") == 0) {
        return &g_console_inode;
    }

    // Week 9 Phase 1: Only support /dev/console
    // Week 10: Will add full path resolution

    return nullptr;  // Not found
}

// ============================================================================
// VFS Read/Write Operations
// ============================================================================

/**
 * @brief Read from inode
 *
 * Week 9 Phase 1: Console read not implemented (returns 0 = EOF)
 * Week 10: Will support reading from serial console
 *
 * @param inode Inode to read from
 * @param buffer Kernel buffer to read into
 * @param count Number of bytes to read
 * @param offset File offset (ignored for devices)
 * @return Number of bytes read, or negative error
 */
ssize_t vfs_read(void* inode, void* buffer, size_t count, uint64_t offset) {
    if (!inode || !buffer) {
        return -EINVAL;
    }

    SimpleInode* si = static_cast<SimpleInode*>(inode);

    if (!si->is_device) {
        // Week 9 Phase 1: No regular file support yet
        return -ENOSYS;
    }

    if (si->is_console) {
        // Week 9 Phase 1: Console input not implemented
        // Reading from console requires:
        // 1. Serial input buffering
        // 2. Line discipline (canonical mode)
        // 3. Interrupt-driven input
        //
        // For now, return 0 (EOF) to indicate no data available
        // Week 10: Will implement full console input

        return 0;  // EOF (no data available)
    }

    return -EIO;  // Unknown device
}

/**
 * @brief Write to inode
 *
 * Week 9 Phase 1: Writes to serial console
 *
 * @param inode Inode to write to
 * @param buffer Kernel buffer to write from
 * @param count Number of bytes to write
 * @param offset File offset (ignored for devices)
 * @return Number of bytes written, or negative error
 */
ssize_t vfs_write(void* inode, const void* buffer, size_t count, uint64_t offset) {
    if (!inode || !buffer) {
        return -EINVAL;
    }

    SimpleInode* si = static_cast<SimpleInode*>(inode);

    if (!si->is_device) {
        // Week 9 Phase 1: No regular file support yet
        return -ENOSYS;
    }

    if (si->is_console) {
        // Write to early serial console
        const char* buf = static_cast<const char*>(buffer);
        for (size_t i = 0; i < count; i++) {
            early_serial.write_char(buf[i]);
        }

        return (ssize_t)count;  // All bytes written
    }

    return -EIO;  // Unknown device
}

// ============================================================================
// VFS File Information
// ============================================================================

/**
 * @brief Get file/device size
 *
 * Week 9 Phase 1: Returns 0 for devices
 *
 * @param inode Inode to query
 * @return File size in bytes (0 for devices)
 */
uint64_t vfs_get_size(void* inode) {
    if (!inode) {
        return 0;
    }

    SimpleInode* si = static_cast<SimpleInode*>(inode);

    if (si->is_device) {
        return 0;  // Devices have no size
    }

    // Week 9 Phase 1: No regular files yet
    return 0;
}

/**
 * @brief Check if inode is a device
 *
 * @param inode Inode to check
 * @return true if device, false otherwise
 */
bool vfs_is_device(void* inode) {
    if (!inode) {
        return false;
    }

    SimpleInode* si = static_cast<SimpleInode*>(inode);
    return si->is_device;
}

/**
 * @brief Check if inode is a directory
 *
 * Week 9 Phase 1: No directory support
 *
 * @param inode Inode to check
 * @return false (no directories yet)
 */
bool vfs_is_directory(void* inode) {
    // Week 9 Phase 1: No directory support
    return false;
}

// ============================================================================
// VFS File Modification (Not Implemented in Phase 1)
// ============================================================================

/**
 * @brief Create new file
 *
 * Week 9 Phase 1: Not implemented
 *
 * @return nullptr (not supported)
 */
void* vfs_create(const char* pathname, uint32_t mode) {
    // Week 9 Phase 1: File creation not supported
    return nullptr;
}

/**
 * @brief Truncate file
 *
 * Week 9 Phase 1: Not implemented
 *
 * @return -ENOSYS (not supported)
 */
int vfs_truncate(void* inode, uint64_t size) {
    // Week 9 Phase 1: Truncation not supported
    return -ENOSYS;
}

/**
 * @brief Delete file
 *
 * Week 9 Phase 1: Not implemented
 *
 * @return -ENOSYS (not supported)
 */
int vfs_unlink(const char* pathname) {
    // Week 9 Phase 1: Deletion not supported
    return -ENOSYS;
}

} // namespace xinim::kernel

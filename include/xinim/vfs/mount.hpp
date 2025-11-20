/**
 * @file mount.hpp
 * @brief Mount Table and Mount Point Management
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Manages the system-wide mount table and provides mount/unmount operations.
 */

#pragma once

#include <xinim/vfs/vfs.hpp>
#include <xinim/vfs/filesystem.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace xinim::vfs {

/**
 * @brief Mount information
 */
struct MountInfo {
    std::string mount_point;      // Where filesystem is mounted (e.g., "/mnt/disk")
    std::string device;           // Device name (e.g., "sda1")
    std::string filesystem_type;  // Filesystem type (e.g., "ext2", "fat32")
    MountOptions options;         // Mount options
    std::unique_ptr<FileSystem> filesystem;  // Filesystem instance
    VNode* root_vnode;            // Root vnode of mounted filesystem
    uint64_t mount_time;          // When filesystem was mounted (timestamp)
    uint32_t mount_id;            // Unique mount ID
};

/**
 * @brief Mount table manager
 *
 * Maintains the system-wide table of mounted filesystems and provides
 * mount/unmount operations with proper validation and error handling.
 */
class MountTable {
public:
    static MountTable& instance();

    /**
     * @brief Mount a filesystem
     * @param device Device to mount (e.g., "sda1", "none" for pseudo-fs)
     * @param mount_point Where to mount (e.g., "/mnt/disk")
     * @param filesystem_type Filesystem type (e.g., "ext2", "tmpfs")
     * @param options Mount options string (e.g., "ro,noexec")
     * @return 0 on success, -errno on error
     */
    int mount(const std::string& device,
              const std::string& mount_point,
              const std::string& filesystem_type,
              const std::string& options = "");

    /**
     * @brief Unmount a filesystem
     * @param mount_point Mount point to unmount
     * @param force Force unmount even if busy
     * @return 0 on success, -errno on error
     */
    int unmount(const std::string& mount_point, bool force = false);

    /**
     * @brief Check if a path is a mount point
     * @param path Path to check
     * @return true if path is a mount point
     */
    bool is_mount_point(const std::string& path) const;

    /**
     * @brief Get mount info for a path
     * @param path Path to check
     * @return Pointer to MountInfo, or nullptr if not found
     */
    const MountInfo* get_mount_info(const std::string& path) const;

    /**
     * @brief Get all mounts
     * @return Vector of all mount points
     */
    std::vector<std::string> get_mount_points() const;

    /**
     * @brief Find mount point for a path
     *
     * Given a path, find the mount point that contains it.
     * For example, "/mnt/disk/foo/bar" might be on mount point "/mnt/disk"
     *
     * @param path Path to search
     * @return Mount point path, or "/" if not found
     */
    std::string find_mount_point(const std::string& path) const;

    /**
     * @brief Get root vnode for a mount point
     * @param mount_point Mount point path
     * @return Root VNode, or nullptr if not mounted
     */
    VNode* get_root_vnode(const std::string& mount_point) const;

    /**
     * @brief Print mount table (for debugging)
     */
    void print_mounts() const;

    /**
     * @brief Sync all mounted filesystems
     * @return 0 on success, -errno on error
     */
    int sync_all();

private:
    MountTable();
    ~MountTable();
    MountTable(const MountTable&) = delete;
    MountTable& operator=(const MountTable&) = delete;

    /**
     * @brief Validate mount point path
     * @return 0 if valid, -errno if invalid
     */
    int validate_mount_point(const std::string& path) const;

    /**
     * @brief Check if filesystem is busy
     * @return true if filesystem has open files or is in use
     */
    bool is_filesystem_busy(const std::string& mount_point) const;

    // Mount table: mount_point -> MountInfo
    std::unordered_map<std::string, std::unique_ptr<MountInfo>> mounts_;
    mutable std::mutex mutex_;
    uint32_t next_mount_id_;
};

} // namespace xinim::vfs

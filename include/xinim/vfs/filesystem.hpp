/**
 * @file filesystem.hpp
 * @brief Filesystem Driver Interface with Block Device Integration
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Provides base classes for filesystem implementations that work with
 * block devices. Integrates VFS layer with storage layer.
 */

#pragma once

#include <xinim/vfs/vfs.hpp>
#include <xinim/block/blockdev.hpp>
#include <memory>
#include <string>
#include <mutex>

namespace xinim::vfs {

/**
 * @brief Mount options
 */
struct MountOptions {
    bool read_only;      // Mount read-only
    bool no_exec;        // No execute permission
    bool no_suid;        // Ignore suid/sgid bits
    bool no_dev;         // No device files
    bool synchronous;    // Synchronous I/O
    bool no_atime;       // Don't update access times

    MountOptions()
        : read_only(false),
          no_exec(false),
          no_suid(false),
          no_dev(false),
          synchronous(false),
          no_atime(false) {}

    // Parse mount options from string (e.g., "ro,noexec,noatime")
    static MountOptions parse(const std::string& options);

    // Convert to flags
    uint32_t to_flags() const;

    // Convert from flags
    static MountOptions from_flags(uint32_t flags);
};

/**
 * @brief Filesystem statistics
 */
struct FilesystemStats {
    uint64_t block_size;        // Filesystem block size
    uint64_t total_blocks;      // Total blocks
    uint64_t free_blocks;       // Free blocks
    uint64_t available_blocks;  // Available blocks (for non-root)
    uint64_t total_inodes;      // Total inodes
    uint64_t free_inodes;       // Free inodes
    uint64_t available_inodes;  // Available inodes (for non-root)
    uint32_t max_filename_len;  // Maximum filename length
};

/**
 * @brief Block-based filesystem driver
 *
 * Base class for filesystems that operate on block devices (ext2, ext4, FAT32, etc.)
 * Provides common infrastructure for block I/O, caching, and mount/unmount.
 */
class BlockFilesystem : public FileSystem {
public:
    BlockFilesystem();
    virtual ~BlockFilesystem();

    // FileSystem interface implementation
    int mount(const std::string& device, uint32_t flags) override;
    int unmount() override;
    VNode* get_root() override;
    int sync() override;

    std::string get_type() const override = 0;  // Must be implemented by subclass

    // Block device access
    std::shared_ptr<block::BlockDevice> get_block_device() const { return block_device_; }
    bool is_mounted() const { return mounted_; }
    bool is_read_only() const { return mount_options_.read_only; }

    // Filesystem statistics
    FilesystemStats get_stats() const;

protected:
    /**
     * @brief Read filesystem superblock and initialize structures
     * Called by mount() after block device is opened
     * @return 0 on success, -errno on error
     */
    virtual int read_superblock() = 0;

    /**
     * @brief Write filesystem superblock
     * Called by sync() and unmount()
     * @return 0 on success, -errno on error
     */
    virtual int write_superblock() = 0;

    /**
     * @brief Create root VNode after successful mount
     * @return Root VNode pointer, or nullptr on error
     */
    virtual VNode* create_root_vnode() = 0;

    /**
     * @brief Flush all dirty data to disk
     * @return 0 on success, -errno on error
     */
    virtual int flush_all() = 0;

    // Block I/O helpers
    int read_block(uint64_t block_num, void* buffer);
    int write_block(uint64_t block_num, const void* buffer);
    int read_blocks(uint64_t start_block, uint32_t count, void* buffer);
    int write_blocks(uint64_t start_block, uint32_t count, const void* buffer);

    // Protected members
    std::shared_ptr<block::BlockDevice> block_device_;
    std::string device_name_;
    MountOptions mount_options_;
    VNode* root_vnode_;
    bool mounted_;
    mutable std::mutex fs_mutex_;

    // Filesystem parameters (set by subclass in read_superblock)
    uint32_t block_size_;
    uint64_t total_blocks_;
    uint64_t free_blocks_;
    uint64_t total_inodes_;
    uint64_t free_inodes_;
};

/**
 * @brief Pseudo-filesystem driver (for proc, dev, tmp, etc.)
 *
 * Base class for in-memory filesystems that don't require block devices.
 */
class PseudoFilesystem : public FileSystem {
public:
    PseudoFilesystem();
    virtual ~PseudoFilesystem();

    // FileSystem interface
    int mount(const std::string& device, uint32_t flags) override;
    int unmount() override;
    VNode* get_root() override;
    int sync() override;

    std::string get_type() const override = 0;

protected:
    /**
     * @brief Initialize filesystem structures
     * @return 0 on success, -errno on error
     */
    virtual int initialize() = 0;

    VNode* root_vnode_;
    bool mounted_;
    mutable std::mutex fs_mutex_;
};

/**
 * @brief Filesystem driver registry
 *
 * Manages registration and creation of filesystem drivers.
 */
class FilesystemRegistry {
public:
    using FilesystemFactory = std::function<std::unique_ptr<FileSystem>()>;

    static FilesystemRegistry& instance();

    /**
     * @brief Register a filesystem type
     * @param type Filesystem type name (e.g., "ext2", "fat32")
     * @param factory Factory function to create filesystem instances
     * @return 0 on success, -errno on error
     */
    int register_filesystem(const std::string& type, FilesystemFactory factory);

    /**
     * @brief Unregister a filesystem type
     * @param type Filesystem type name
     * @return 0 on success, -errno on error
     */
    int unregister_filesystem(const std::string& type);

    /**
     * @brief Create filesystem instance
     * @param type Filesystem type name
     * @return Filesystem instance, or nullptr if type not found
     */
    std::unique_ptr<FileSystem> create_filesystem(const std::string& type);

    /**
     * @brief Get list of registered filesystem types
     */
    std::vector<std::string> get_registered_types() const;

private:
    FilesystemRegistry() = default;
    ~FilesystemRegistry() = default;
    FilesystemRegistry(const FilesystemRegistry&) = delete;
    FilesystemRegistry& operator=(const FilesystemRegistry&) = delete;

    std::unordered_map<std::string, FilesystemFactory> factories_;
    mutable std::mutex mutex_;
};

} // namespace xinim::vfs

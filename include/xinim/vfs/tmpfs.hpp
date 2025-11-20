/**
 * @file tmpfs.hpp
 * @brief tmpfs - Temporary In-Memory Filesystem
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Implements a production-grade temporary filesystem that stores all data
 * in RAM with configurable size limits. Used for /tmp, /dev/shm, etc.
 */

#pragma once

#include <xinim/vfs/vfs.hpp>
#include <xinim/vfs/filesystem.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace xinim::vfs {

// Forward declarations
class TmpfsFilesystem;
class TmpfsNode;

/**
 * @brief tmpfs configuration
 */
struct TmpfsConfig {
    uint64_t max_size;       // Maximum filesystem size in bytes (0 = unlimited)
    uint64_t max_inodes;     // Maximum number of inodes (0 = unlimited)
    uint32_t default_mode;   // Default permissions for new files

    TmpfsConfig()
        : max_size(256 * 1024 * 1024),  // 256 MB default
          max_inodes(4096),               // 4K inodes default
          default_mode(0755) {}
};

/**
 * @brief tmpfs VNode
 *
 * Represents a file, directory, or symlink in tmpfs.
 * All data is stored in RAM.
 */
class TmpfsNode : public VNode, public std::enable_shared_from_this<TmpfsNode> {
public:
    TmpfsNode(TmpfsFilesystem* fs, FileType type, uint64_t inode, uint32_t mode);
    virtual ~TmpfsNode();

    // VNode interface
    int read(void* buffer, size_t size, uint64_t offset) override;
    int write(const void* buffer, size_t size, uint64_t offset) override;
    int truncate(uint64_t size) override;
    int sync() override;

    std::vector<std::string> readdir() override;
    VNode* lookup(const std::string& name) override;
    int create(const std::string& name, FilePermissions perms) override;
    int mkdir(const std::string& name, FilePermissions perms) override;
    int remove(const std::string& name) override;
    int rmdir(const std::string& name) override;
    int link(const std::string& name, VNode* target) override;
    int symlink(const std::string& name, const std::string& target) override;
    int rename(const std::string& oldname, const std::string& newname) override;

    FileAttributes get_attributes() override;
    int set_attributes(const FileAttributes& attrs) override;

    void ref() override;
    void unref() override;

    // tmpfs-specific
    std::string read_link() const;
    uint64_t get_inode() const { return inode_; }
    FileType get_type() const { return type_; }

private:
    TmpfsFilesystem* tmpfs_;
    FileType type_;
    uint64_t inode_;
    uint32_t mode_;
    uint32_t uid_;
    uint32_t gid_;
    uint64_t size_;
    uint32_t nlinks_;
    int ref_count_;

    // Timestamps (in microseconds since epoch)
    uint64_t atime_;
    uint64_t mtime_;
    uint64_t ctime_;

    // File data (for regular files)
    std::vector<uint8_t> data_;

    // Directory entries (for directories)
    std::unordered_map<std::string, std::shared_ptr<TmpfsNode>> children_;

    // Symlink target (for symlinks)
    std::string symlink_target_;

    mutable std::mutex mutex_;

    // Helper methods
    void update_atime();
    void update_mtime();
    void update_ctime();
    uint64_t get_current_time();
};

/**
 * @brief tmpfs filesystem
 *
 * In-memory filesystem with size limits to prevent OOM.
 * Faster than disk-based filesystems but not persistent.
 */
class TmpfsFilesystem : public PseudoFilesystem {
public:
    TmpfsFilesystem();
    explicit TmpfsFilesystem(const TmpfsConfig& config);
    virtual ~TmpfsFilesystem();

    // FileSystem interface
    std::string get_type() const override { return "tmpfs"; }
    uint64_t get_total_blocks() const override;
    uint64_t get_free_blocks() const override;
    uint64_t get_total_inodes() const override;
    uint64_t get_free_inodes() const override;

    // tmpfs-specific
    uint64_t get_used_bytes() const { return used_bytes_; }
    uint64_t get_max_bytes() const { return config_.max_size; }
    uint64_t get_used_inodes() const { return used_inodes_; }

    // Internal methods (called by TmpfsNode)
    uint64_t allocate_inode();
    void free_inode();
    bool allocate_space(size_t bytes);
    void free_space(size_t bytes);

    std::shared_ptr<TmpfsNode> create_node(FileType type, uint32_t mode);

protected:
    // PseudoFilesystem interface
    int initialize() override;

private:
    TmpfsConfig config_;
    std::shared_ptr<TmpfsNode> root_node_;

    // Resource tracking
    uint64_t next_inode_;
    uint64_t used_inodes_;
    uint64_t used_bytes_;

    mutable std::mutex fs_mutex_;

    friend class TmpfsNode;
};

} // namespace xinim::vfs

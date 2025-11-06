/**
 * @file vfs.hpp
 * @brief Virtual File System Layer for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Based on modern Unix VFS design (BSD, Linux)
 * Provides filesystem-independent interface
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace xinim::vfs {

/**
 * @brief File Types
 */
enum class FileType {
    REGULAR,      // Regular file
    DIRECTORY,    // Directory
    SYMLINK,      // Symbolic link
    BLOCK_DEVICE, // Block device
    CHAR_DEVICE,  // Character device
    FIFO,         // Named pipe
    SOCKET,       // Socket
    UNKNOWN,      // Unknown type
};

/**
 * @brief File Permissions (Unix-style)
 */
struct FilePermissions {
    uint16_t mode;  // Unix permission bits
    
    static constexpr uint16_t OWNER_READ   = 0400;
    static constexpr uint16_t OWNER_WRITE  = 0200;
    static constexpr uint16_t OWNER_EXEC   = 0100;
    static constexpr uint16_t GROUP_READ   = 0040;
    static constexpr uint16_t GROUP_WRITE  = 0020;
    static constexpr uint16_t GROUP_EXEC   = 0010;
    static constexpr uint16_t OTHER_READ   = 0004;
    static constexpr uint16_t OTHER_WRITE  = 0002;
    static constexpr uint16_t OTHER_EXEC   = 0001;
    
    bool can_read(uint32_t uid, uint32_t gid, uint32_t file_uid, uint32_t file_gid) const;
    bool can_write(uint32_t uid, uint32_t gid, uint32_t file_uid, uint32_t file_gid) const;
    bool can_execute(uint32_t uid, uint32_t gid, uint32_t file_uid, uint32_t file_gid) const;
};

/**
 * @brief File Attributes
 */
struct FileAttributes {
    uint64_t inode_number;
    FileType type;
    FilePermissions permissions;
    uint32_t uid;  // Owner user ID
    uint32_t gid;  // Owner group ID
    uint64_t size;
    uint64_t blocks;
    uint32_t block_size;
    
    // Timestamps
    uint64_t atime;  // Access time
    uint64_t mtime;  // Modification time
    uint64_t ctime;  // Change time (inode)
    
    // Links
    uint32_t nlinks;  // Hard link count
};

/**
 * @brief VNode (Virtual Node)
 * 
 * Filesystem-independent representation of a file
 */
class VNode {
public:
    virtual ~VNode() = default;
    
    // File operations
    virtual int read(void* buffer, size_t size, uint64_t offset) = 0;
    virtual int write(const void* buffer, size_t size, uint64_t offset) = 0;
    virtual int truncate(uint64_t size) = 0;
    virtual int sync() = 0;
    
    // Directory operations
    virtual std::vector<std::string> readdir() = 0;
    virtual VNode* lookup(const std::string& name) = 0;
    virtual int create(const std::string& name, FilePermissions perms) = 0;
    virtual int mkdir(const std::string& name, FilePermissions perms) = 0;
    virtual int remove(const std::string& name) = 0;
    virtual int rmdir(const std::string& name) = 0;
    virtual int link(const std::string& name, VNode* target) = 0;
    virtual int symlink(const std::string& name, const std::string& target) = 0;
    virtual int rename(const std::string& oldname, const std::string& newname) = 0;
    
    // Attributes
    virtual FileAttributes get_attributes() = 0;
    virtual int set_attributes(const FileAttributes& attrs) = 0;
    
    // Reference counting
    virtual void ref() = 0;
    virtual void unref() = 0;
    
    // Filesystem pointer
    class FileSystem* get_filesystem() const { return fs_; }
    
protected:
    FileSystem* fs_ = nullptr;
    uint64_t inode_number_ = 0;
};

/**
 * @brief Mount Point
 */
struct MountPoint {
    std::string path;
    class FileSystem* fs;
    VNode* root_vnode;
    uint32_t flags;
    
    static constexpr uint32_t RDONLY  = 0x0001;
    static constexpr uint32_t NOEXEC  = 0x0002;
    static constexpr uint32_t NOSUID  = 0x0004;
    static constexpr uint32_t NODEV   = 0x0008;
};

/**
 * @brief File System Interface
 */
class FileSystem {
public:
    virtual ~FileSystem() = default;
    
    // Mounting
    virtual int mount(const std::string& device, uint32_t flags) = 0;
    virtual int unmount() = 0;
    
    // Root vnode
    virtual VNode* get_root() = 0;
    
    // Filesystem operations
    virtual int sync() = 0;
    
    // Filesystem info
    virtual std::string get_type() const = 0;
    virtual uint64_t get_total_blocks() const = 0;
    virtual uint64_t get_free_blocks() const = 0;
    virtual uint64_t get_total_inodes() const = 0;
    virtual uint64_t get_free_inodes() const = 0;
};

/**
 * @brief VFS Manager (Singleton)
 */
class VFS {
public:
    static VFS& instance();
    
    // Mount management
    int mount(const std::string& device, const std::string& path,
             const std::string& fstype, uint32_t flags);
    int unmount(const std::string& path);
    std::vector<MountPoint> get_mounts() const;
    
    // File operations (path-based)
    VNode* open(const std::string& path, int flags, FilePermissions mode);
    int close(VNode* vnode);
    int read(VNode* vnode, void* buffer, size_t size, uint64_t offset);
    int write(VNode* vnode, const void* buffer, size_t size, uint64_t offset);
    int stat(const std::string& path, FileAttributes& attrs);
    int lstat(const std::string& path, FileAttributes& attrs);
    
    // Directory operations
    std::vector<std::string> readdir(const std::string& path);
    int mkdir(const std::string& path, FilePermissions mode);
    int rmdir(const std::string& path);
    int unlink(const std::string& path);
    int link(const std::string& oldpath, const std::string& newpath);
    int symlink(const std::string& target, const std::string& linkpath);
    int rename(const std::string& oldpath, const std::string& newpath);
    
    // Path resolution
    VNode* resolve_path(const std::string& path, bool follow_symlinks = true);
    std::string normalize_path(const std::string& path);
    std::string join_path(const std::string& base, const std::string& relative);
    
    // Filesystem registration
    bool register_filesystem(const std::string& type,
                           std::function<FileSystem*()> factory);
    bool unregister_filesystem(const std::string& type);
    
    // Current working directory
    std::string get_cwd() const;
    int chdir(const std::string& path);
    
    // Root directory
    VNode* get_root() const { return root_vnode_; }

private:
    VFS() = default;
    ~VFS() = default;
    
    // No copy
    VFS(const VFS&) = delete;
    VFS& operator=(const VFS&) = delete;
    
    // Internal path resolution
    VNode* resolve_component(VNode* dir, const std::string& component);
    VNode* follow_symlink(VNode* symlink, int depth = 0);
    
    // Mount table
    std::vector<MountPoint> mounts_;
    VNode* root_vnode_ = nullptr;
    
    // Filesystem types
    std::unordered_map<std::string, std::function<FileSystem*()>> fs_types_;
    
    // Current working directory (per process - simplified)
    std::string cwd_ = "/";
    
    // Maximum symlink follow depth
    static constexpr int MAX_SYMLINK_DEPTH = 8;
};

/**
 * @brief File Descriptor Table
 */
class FileDescriptorTable {
public:
    struct OpenFile {
        VNode* vnode;
        uint64_t offset;
        uint32_t flags;
        uint32_t ref_count;
    };
    
    int allocate_fd(VNode* vnode, uint32_t flags);
    OpenFile* get_file(int fd);
    int close_fd(int fd);
    int dup_fd(int oldfd);
    int dup2_fd(int oldfd, int newfd);
    
private:
    static constexpr int MAX_FDS = 1024;
    OpenFile files_[MAX_FDS];
};

} // namespace xinim::vfs

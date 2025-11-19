/**
 * @file ramfs.hpp
 * @brief RAM Filesystem - Simple in-memory filesystem for XINIM VFS
 *
 * ramfs provides a simple in-memory filesystem with support for:
 * - Files and directories
 * - POSIX permissions
 * - File metadata (size, timestamps, owner)
 * - Hard links (reference counting)
 *
 * This is the initial filesystem implementation for XINIM. It stores
 * all data in kernel memory and is lost on reboot.
 *
 * @ingroup vfs
 */

#ifndef XINIM_VFS_RAMFS_HPP
#define XINIM_VFS_RAMFS_HPP

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../include/defs.hpp"

namespace xinim::vfs {

/* Forward declarations */
class RamfsNode;
class RamfsFile;
class RamfsDir;

/* ========================================================================
 * File Types and Constants
 * ======================================================================== */

/**
 * @brief File type flags (compatible with POSIX stat st_mode)
 */
enum class FileType : uint16_t {
    S_IFREG  = 0x8000,       /**< Regular file */
    S_IFDIR  = 0x4000,       /**< Directory */
    S_IFLNK  = 0xA000,       /**< Symbolic link */
    S_IFBLK  = 0x6000,       /**< Block device */
    S_IFCHR  = 0x2000,       /**< Character device */
    S_IFIFO  = 0x1000,       /**< FIFO (named pipe) */
    S_IFSOCK = 0xC000,       /**< Socket */
};

/**
 * @brief Permission flags (POSIX)
 */
enum class Permission : uint16_t {
    S_ISUID  = 0x0800,       /**< Set-user-ID */
    S_ISGID  = 0x0400,       /**< Set-group-ID */
    S_ISVTX  = 0x0200,       /**< Sticky bit */
    S_IRUSR  = 0x0100,       /**< Owner read */
    S_IWUSR  = 0x0080,       /**< Owner write */
    S_IXUSR  = 0x0040,       /**< Owner execute */
    S_IRGRP  = 0x0020,       /**< Group read */
    S_IWGRP  = 0x0010,       /**< Group write */
    S_IXGRP  = 0x0008,       /**< Group execute */
    S_IROTH  = 0x0004,       /**< Others read */
    S_IWOTH  = 0x0002,       /**< Others write */
    S_IXOTH  = 0x0001,       /**< Others execute */
};

/**
 * @brief Default permissions
 */
constexpr uint16_t DEFAULT_FILE_MODE = 0644;  /**< rw-r--r-- */
constexpr uint16_t DEFAULT_DIR_MODE  = 0755;  /**< rwxr-xr-x */

/**
 * @brief Maximum filename length
 */
constexpr size_t MAX_FILENAME_LEN = 255;

/* ========================================================================
 * RamfsNode - Base class for filesystem nodes
 * ======================================================================== */

/**
 * @brief Base class for all ramfs filesystem nodes
 *
 * Represents a generic inode (file, directory, symlink, etc.)
 * with metadata and operations.
 */
class RamfsNode {
public:
    /**
     * @brief Node metadata
     */
    struct Metadata {
        uint64_t inode;          /**< Inode number (unique) */
        uint16_t mode;           /**< File type and permissions */
        uint32_t nlink;          /**< Number of hard links */
        uint32_t uid;            /**< Owner user ID */
        uint32_t gid;            /**< Owner group ID */
        uint64_t size;           /**< File size in bytes */
        int64_t atime;           /**< Access time (seconds since epoch) */
        int64_t mtime;           /**< Modification time */
        int64_t ctime;           /**< Status change time */

        Metadata() : inode(0), mode(0), nlink(1), uid(0), gid(0),
                     size(0), atime(0), mtime(0), ctime(0) {}
    };

protected:
    Metadata meta_;              /**< Node metadata */
    std::string name_;           /**< Node name (filename) */
    RamfsDir* parent_;           /**< Parent directory (nullptr for root) */

public:
    RamfsNode(uint64_t inode, const std::string& name, uint16_t mode,
              RamfsDir* parent = nullptr)
        : name_(name), parent_(parent) {
        meta_.inode = inode;
        meta_.mode = mode;
        meta_.uid = 0;  // TODO: Get from current process
        meta_.gid = 0;
        update_times();
    }

    virtual ~RamfsNode() = default;

    /* Metadata accessors */
    const Metadata& metadata() const noexcept { return meta_; }
    uint64_t inode() const noexcept { return meta_.inode; }
    uint16_t mode() const noexcept { return meta_.mode; }
    uint64_t size() const noexcept { return meta_.size; }
    const std::string& name() const noexcept { return name_; }
    RamfsDir* parent() const noexcept { return parent_; }

    /* Type checking */
    bool is_file() const noexcept {
        return (meta_.mode & 0xF000) == static_cast<uint16_t>(FileType::S_IFREG);
    }
    bool is_dir() const noexcept {
        return (meta_.mode & 0xF000) == static_cast<uint16_t>(FileType::S_IFDIR);
    }
    bool is_symlink() const noexcept {
        return (meta_.mode & 0xF000) == static_cast<uint16_t>(FileType::S_IFLNK);
    }

    /* Permission checking */
    bool can_read(uint32_t uid, uint32_t gid) const noexcept {
        if (uid == 0) return true;  // Root can always read
        if (uid == meta_.uid) return meta_.mode & static_cast<uint16_t>(Permission::S_IRUSR);
        if (gid == meta_.gid) return meta_.mode & static_cast<uint16_t>(Permission::S_IRGRP);
        return meta_.mode & static_cast<uint16_t>(Permission::S_IROTH);
    }

    bool can_write(uint32_t uid, uint32_t gid) const noexcept {
        if (uid == 0) return true;  // Root can always write
        if (uid == meta_.uid) return meta_.mode & static_cast<uint16_t>(Permission::S_IWUSR);
        if (gid == meta_.gid) return meta_.mode & static_cast<uint16_t>(Permission::S_IWGRP);
        return meta_.mode & static_cast<uint16_t>(Permission::S_IWOTH);
    }

    bool can_execute(uint32_t uid, uint32_t gid) const noexcept {
        if (uid == 0) return true;  // Root can always execute
        if (uid == meta_.uid) return meta_.mode & static_cast<uint16_t>(Permission::S_IXUSR);
        if (gid == meta_.gid) return meta_.mode & static_cast<uint16_t>(Permission::S_IXGRP);
        return meta_.mode & static_cast<uint16_t>(Permission::S_IXOTH);
    }

    /* Metadata modification */
    void set_mode(uint16_t mode) noexcept {
        meta_.mode = (meta_.mode & 0xF000) | (mode & 0x0FFF);
        update_ctime();
    }

    void set_owner(uint32_t uid, uint32_t gid) noexcept {
        meta_.uid = uid;
        meta_.gid = gid;
        update_ctime();
    }

    void inc_nlink() noexcept { meta_.nlink++; update_ctime(); }
    void dec_nlink() noexcept { if (meta_.nlink > 0) meta_.nlink--; update_ctime(); }

    /* Time management */
    void update_atime() noexcept {
        meta_.atime = get_current_time();
    }

    void update_mtime() noexcept {
        meta_.mtime = get_current_time();
        update_ctime();
    }

    void update_ctime() noexcept {
        meta_.ctime = get_current_time();
    }

    void update_times() noexcept {
        int64_t now = get_current_time();
        meta_.atime = now;
        meta_.mtime = now;
        meta_.ctime = now;
    }

protected:
    void set_size(uint64_t size) noexcept {
        meta_.size = size;
        update_mtime();
    }

    int64_t get_current_time() const noexcept {
        // TODO: Get real time from kernel
        return 0;  // Stub: return 0 for now
    }
};

/* ========================================================================
 * RamfsFile - Regular file
 * ======================================================================== */

/**
 * @brief Regular file node with data storage
 */
class RamfsFile : public RamfsNode {
private:
    std::vector<uint8_t> data_;  /**< File contents */

public:
    RamfsFile(uint64_t inode, const std::string& name, uint16_t mode = DEFAULT_FILE_MODE,
              RamfsDir* parent = nullptr)
        : RamfsNode(inode, name, static_cast<uint16_t>(FileType::S_IFREG) | mode, parent) {}

    /**
     * @brief Read data from file
     * @param buf Buffer to read into
     * @param count Number of bytes to read
     * @param offset Offset to start reading from
     * @return Number of bytes read, or -1 on error
     */
    int64_t read(void* buf, uint64_t count, uint64_t offset) {
        if (offset >= data_.size()) return 0;  // EOF

        uint64_t available = data_.size() - offset;
        uint64_t to_read = (count < available) ? count : available;

        std::memcpy(buf, data_.data() + offset, to_read);
        update_atime();
        return static_cast<int64_t>(to_read);
    }

    /**
     * @brief Write data to file
     * @param buf Buffer to write from
     * @param count Number of bytes to write
     * @param offset Offset to start writing at
     * @return Number of bytes written, or -1 on error
     */
    int64_t write(const void* buf, uint64_t count, uint64_t offset) {
        // Resize if necessary
        if (offset + count > data_.size()) {
            data_.resize(offset + count);
        }

        std::memcpy(data_.data() + offset, buf, count);
        set_size(data_.size());
        return static_cast<int64_t>(count);
    }

    /**
     * @brief Truncate file to specified size
     * @param size New file size
     * @return 0 on success, -1 on error
     */
    int truncate(uint64_t size) {
        data_.resize(size);
        set_size(size);
        return 0;
    }

    /**
     * @brief Get file data (for debugging)
     */
    const std::vector<uint8_t>& data() const noexcept { return data_; }
};

/* ========================================================================
 * RamfsDir - Directory
 * ======================================================================== */

/**
 * @brief Directory node with child entries
 */
class RamfsDir : public RamfsNode {
private:
    std::unordered_map<std::string, std::shared_ptr<RamfsNode>> entries_;

public:
    RamfsDir(uint64_t inode, const std::string& name, uint16_t mode = DEFAULT_DIR_MODE,
             RamfsDir* parent = nullptr)
        : RamfsNode(inode, name, static_cast<uint16_t>(FileType::S_IFDIR) | mode, parent) {}

    /**
     * @brief Look up child node by name
     * @param name Child name
     * @return Pointer to child node, or nullptr if not found
     */
    std::shared_ptr<RamfsNode> lookup(const std::string& name) const {
        auto it = entries_.find(name);
        return (it != entries_.end()) ? it->second : nullptr;
    }

    /**
     * @brief Add child node
     * @param name Child name
     * @param node Child node
     * @return 0 on success, -1 if name already exists
     */
    int add_entry(const std::string& name, std::shared_ptr<RamfsNode> node) {
        if (name.empty() || name.length() > MAX_FILENAME_LEN) return -1;
        if (name == "." || name == "..") return -1;  // Reserved names
        if (entries_.count(name)) return -1;  // Already exists

        entries_[name] = node;
        update_mtime();
        return 0;
    }

    /**
     * @brief Remove child node
     * @param name Child name
     * @return 0 on success, -1 if not found
     */
    int remove_entry(const std::string& name) {
        auto it = entries_.find(name);
        if (it == entries_.end()) return -1;

        entries_.erase(it);
        update_mtime();
        return 0;
    }

    /**
     * @brief Check if directory is empty (excluding . and ..)
     */
    bool is_empty() const noexcept {
        return entries_.empty();
    }

    /**
     * @brief Get all child names
     */
    std::vector<std::string> list_entries() const {
        std::vector<std::string> names;
        names.reserve(entries_.size() + 2);
        names.push_back(".");
        names.push_back("..");
        for (const auto& [name, _] : entries_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Get entry map (for iteration)
     */
    const std::unordered_map<std::string, std::shared_ptr<RamfsNode>>& entries() const noexcept {
        return entries_;
    }
};

/* ========================================================================
 * RamfsFilesystem - Filesystem management
 * ======================================================================== */

/**
 * @brief RAM filesystem manager
 *
 * Manages the root directory and inode allocation for the entire filesystem.
 */
class RamfsFilesystem {
private:
    std::shared_ptr<RamfsDir> root_;  /**< Root directory */
    uint64_t next_inode_;             /**< Next inode number */

public:
    RamfsFilesystem() : next_inode_(1) {
        // Create root directory with inode 1
        root_ = std::make_shared<RamfsDir>(allocate_inode(), "/", DEFAULT_DIR_MODE, nullptr);
    }

    /**
     * @brief Get root directory
     */
    std::shared_ptr<RamfsDir> root() const noexcept { return root_; }

    /**
     * @brief Allocate a new inode number
     */
    uint64_t allocate_inode() noexcept {
        return next_inode_++;
    }

    /**
     * @brief Create a new file
     * @param parent Parent directory
     * @param name File name
     * @param mode File permissions
     * @return Pointer to new file, or nullptr on error
     */
    std::shared_ptr<RamfsFile> create_file(std::shared_ptr<RamfsDir> parent,
                                            const std::string& name,
                                            uint16_t mode = DEFAULT_FILE_MODE) {
        if (!parent) return nullptr;
        if (parent->lookup(name)) return nullptr;  // Already exists

        auto file = std::make_shared<RamfsFile>(allocate_inode(), name, mode, parent.get());
        if (parent->add_entry(name, file) != 0) return nullptr;

        return file;
    }

    /**
     * @brief Create a new directory
     * @param parent Parent directory
     * @param name Directory name
     * @param mode Directory permissions
     * @return Pointer to new directory, or nullptr on error
     */
    std::shared_ptr<RamfsDir> create_dir(std::shared_ptr<RamfsDir> parent,
                                          const std::string& name,
                                          uint16_t mode = DEFAULT_DIR_MODE) {
        if (!parent) return nullptr;
        if (parent->lookup(name)) return nullptr;  // Already exists

        auto dir = std::make_shared<RamfsDir>(allocate_inode(), name, mode, parent.get());
        if (parent->add_entry(name, dir) != 0) return nullptr;

        parent->inc_nlink();  // ".." link from child
        return dir;
    }

    /**
     * @brief Remove a node (file or empty directory)
     * @param parent Parent directory
     * @param name Node name
     * @return 0 on success, -1 on error
     */
    int remove_node(std::shared_ptr<RamfsDir> parent, const std::string& name) {
        if (!parent) return -1;

        auto node = parent->lookup(name);
        if (!node) return -1;  // Not found

        // Check if it's a directory and not empty
        if (node->is_dir()) {
            auto dir = std::static_pointer_cast<RamfsDir>(node);
            if (!dir->is_empty()) return -1;  // Directory not empty
            parent->dec_nlink();  // Remove ".." link
        }

        node->dec_nlink();
        if (node->metadata().nlink == 0) {
            return parent->remove_entry(name);
        }

        return 0;
    }
};

} // namespace xinim::vfs

#endif /* XINIM_VFS_RAMFS_HPP */

/**
 * @file tmpfs.cpp
 * @brief tmpfs Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/tmpfs.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <algorithm>

// Error codes
#define EINVAL  22  // Invalid argument
#define ENOENT  2   // No such file or directory
#define EEXIST  17  // File exists
#define ENOTDIR 20  // Not a directory
#define EISDIR  21  // Is a directory
#define ENOTEMPTY 39 // Directory not empty
#define ENOSPC  28  // No space left on device
#define ENOMEM  12  // Out of memory

namespace xinim::vfs {

// ============================================================================
// TmpfsNode Implementation
// ============================================================================

TmpfsNode::TmpfsNode(TmpfsFilesystem* fs, FileType type, uint64_t inode, uint32_t mode)
    : tmpfs_(fs),
      type_(type),
      inode_(inode),
      mode_(mode),
      uid_(0),
      gid_(0),
      size_(0),
      nlinks_(1),
      ref_count_(1) {

    uint64_t now = get_current_time();
    atime_ = now;
    mtime_ = now;
    ctime_ = now;

    fs_ = fs;
    inode_number_ = inode;
}

TmpfsNode::~TmpfsNode() {
    // Free space when node is destroyed
    if (!data_.empty()) {
        tmpfs_->free_space(data_.capacity());
    }
}

int TmpfsNode::read(void* buffer, size_t size, uint64_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::REGULAR) {
        return -EINVAL;
    }

    if (offset >= size_) {
        return 0;  // EOF
    }

    size_t to_read = std::min(size, static_cast<size_t>(size_ - offset));
    memcpy(buffer, data_.data() + offset, to_read);

    update_atime();
    return static_cast<int>(to_read);
}

int TmpfsNode::write(const void* buffer, size_t size, uint64_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::REGULAR) {
        return -EINVAL;
    }

    // Calculate new size needed
    uint64_t new_size = offset + size;

    // Check if we need to allocate more space
    if (new_size > data_.capacity()) {
        size_t additional = new_size - data_.capacity();
        if (!tmpfs_->allocate_space(additional)) {
            return -ENOSPC;
        }

        try {
            data_.reserve(new_size);
        } catch (...) {
            tmpfs_->free_space(additional);
            return -ENOMEM;
        }
    }

    // Resize if needed
    if (new_size > data_.size()) {
        data_.resize(new_size, 0);
        size_ = new_size;
    }

    // Write data
    memcpy(data_.data() + offset, buffer, size);

    update_mtime();
    update_ctime();
    return static_cast<int>(size);
}

int TmpfsNode::truncate(uint64_t new_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::REGULAR) {
        return -EINVAL;
    }

    if (new_size > data_.capacity()) {
        // Need to allocate more space
        size_t additional = new_size - data_.capacity();
        if (!tmpfs_->allocate_space(additional)) {
            return -ENOSPC;
        }

        try {
            data_.reserve(new_size);
        } catch (...) {
            tmpfs_->free_space(additional);
            return -ENOMEM;
        }
    } else if (new_size < data_.capacity()) {
        // Freeing space
        size_t freed = data_.capacity() - new_size;
        data_.resize(new_size);
        data_.shrink_to_fit();
        tmpfs_->free_space(freed);
    }

    data_.resize(new_size, 0);
    size_ = new_size;

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::sync() {
    // tmpfs is in-memory, nothing to sync
    return 0;
}

std::vector<std::string> TmpfsNode::readdir() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return {};
    }

    std::vector<std::string> entries;
    entries.reserve(children_.size() + 2);

    entries.push_back(".");
    entries.push_back("..");

    for (const auto& [name, node] : children_) {
        entries.push_back(name);
    }

    update_atime();
    return entries;
}

VNode* TmpfsNode::lookup(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return nullptr;
    }

    if (name == ".") {
        return this;
    }

    if (name == "..") {
        // TODO: Implement parent tracking
        return this;  // For now, return self
    }

    auto it = children_.find(name);
    if (it != children_.end()) {
        update_atime();
        return it->second.get();
    }

    return nullptr;
}

int TmpfsNode::create(const std::string& name, FilePermissions perms) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    if (children_.find(name) != children_.end()) {
        return -EEXIST;
    }

    // Create new file node
    auto node = tmpfs_->create_node(FileType::REGULAR, perms.mode);
    if (!node) {
        return -ENOSPC;
    }

    children_[name] = node;
    nlinks_++;

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::mkdir(const std::string& name, FilePermissions perms) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    if (children_.find(name) != children_.end()) {
        return -EEXIST;
    }

    // Create new directory node
    auto node = tmpfs_->create_node(FileType::DIRECTORY, perms.mode);
    if (!node) {
        return -ENOSPC;
    }

    children_[name] = node;
    nlinks_++;

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::remove(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    auto it = children_.find(name);
    if (it == children_.end()) {
        return -ENOENT;
    }

    if (it->second->get_type() == FileType::DIRECTORY) {
        return -EISDIR;
    }

    children_.erase(it);
    nlinks_--;
    tmpfs_->free_inode();

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::rmdir(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    auto it = children_.find(name);
    if (it == children_.end()) {
        return -ENOENT;
    }

    if (it->second->get_type() != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    // Check if directory is empty
    if (!it->second->children_.empty()) {
        return -ENOTEMPTY;
    }

    children_.erase(it);
    nlinks_--;
    tmpfs_->free_inode();

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::link(const std::string& name, VNode* target) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    if (children_.find(name) != children_.end()) {
        return -EEXIST;
    }

    // Cast to TmpfsNode
    TmpfsNode* tmpfs_target = dynamic_cast<TmpfsNode*>(target);
    if (!tmpfs_target) {
        return -EINVAL;
    }

    if (tmpfs_target->get_type() == FileType::DIRECTORY) {
        return -EINVAL;  // Can't hard link directories
    }

    // Share the same node
    children_[name] = tmpfs_target->shared_from_this();
    tmpfs_target->nlinks_++;
    nlinks_++;

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::symlink(const std::string& name, const std::string& target) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    if (children_.find(name) != children_.end()) {
        return -EEXIST;
    }

    // Create symlink node
    auto node = tmpfs_->create_node(FileType::SYMLINK, 0777);
    if (!node) {
        return -ENOSPC;
    }

    node->symlink_target_ = target;
    children_[name] = node;
    nlinks_++;

    update_mtime();
    update_ctime();
    return 0;
}

int TmpfsNode::rename(const std::string& oldname, const std::string& newname) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (type_ != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    auto old_it = children_.find(oldname);
    if (old_it == children_.end()) {
        return -ENOENT;
    }

    // Check if newname exists
    auto new_it = children_.find(newname);
    if (new_it != children_.end()) {
        // Target exists - need to remove it first
        // For simplicity, just fail if target exists
        return -EEXIST;
    }

    // Move the node
    children_[newname] = old_it->second;
    children_.erase(old_it);

    update_mtime();
    update_ctime();
    return 0;
}

FileAttributes TmpfsNode::get_attributes() {
    std::lock_guard<std::mutex> lock(mutex_);

    FileAttributes attrs = {};
    attrs.inode_number = inode_;
    attrs.type = type_;
    attrs.permissions.mode = mode_;
    attrs.uid = uid_;
    attrs.gid = gid_;
    attrs.size = size_;
    attrs.blocks = (size_ + 511) / 512;  // 512-byte blocks
    attrs.block_size = 512;
    attrs.atime = atime_;
    attrs.mtime = mtime_;
    attrs.ctime = ctime_;
    attrs.nlinks = nlinks_;

    return attrs;
}

int TmpfsNode::set_attributes(const FileAttributes& attrs) {
    std::lock_guard<std::mutex> lock(mutex_);

    mode_ = attrs.permissions.mode;
    uid_ = attrs.uid;
    gid_ = attrs.gid;

    update_ctime();
    return 0;
}

void TmpfsNode::ref() {
    std::lock_guard<std::mutex> lock(mutex_);
    ref_count_++;
}

void TmpfsNode::unref() {
    std::lock_guard<std::mutex> lock(mutex_);
    ref_count_--;
    // Note: Node cleanup handled by shared_ptr
}

std::string TmpfsNode::read_link() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return symlink_target_;
}

void TmpfsNode::update_atime() {
    atime_ = get_current_time();
}

void TmpfsNode::update_mtime() {
    mtime_ = get_current_time();
}

void TmpfsNode::update_ctime() {
    ctime_ = get_current_time();
}

uint64_t TmpfsNode::get_current_time() {
    // TODO: Use actual system time
    // For now, use a simple counter
    static uint64_t counter = 1000000000;
    return counter++;
}

// ============================================================================
// TmpfsFilesystem Implementation
// ============================================================================

TmpfsFilesystem::TmpfsFilesystem()
    : TmpfsFilesystem(TmpfsConfig()) {
}

TmpfsFilesystem::TmpfsFilesystem(const TmpfsConfig& config)
    : config_(config),
      next_inode_(1),
      used_inodes_(0),
      used_bytes_(0) {
}

TmpfsFilesystem::~TmpfsFilesystem() {
    // Cleanup handled by shared_ptr
}

uint64_t TmpfsFilesystem::get_total_blocks() const {
    return config_.max_size / 512;
}

uint64_t TmpfsFilesystem::get_free_blocks() const {
    return (config_.max_size - used_bytes_) / 512;
}

uint64_t TmpfsFilesystem::get_total_inodes() const {
    return config_.max_inodes;
}

uint64_t TmpfsFilesystem::get_free_inodes() const {
    return config_.max_inodes - used_inodes_;
}

int TmpfsFilesystem::initialize() {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    LOG_INFO("tmpfs: Initializing (max size: %lu MB, max inodes: %lu)",
             config_.max_size / (1024 * 1024), config_.max_inodes);

    // Create root directory
    uint64_t root_inode = allocate_inode();
    root_node_ = std::make_shared<TmpfsNode>(this, FileType::DIRECTORY, root_inode, 0755);
    if (!root_node_) {
        LOG_ERROR("tmpfs: Failed to create root directory");
        return -ENOMEM;
    }

    root_vnode_ = root_node_.get();

    LOG_INFO("tmpfs: Initialized successfully");
    return 0;
}

uint64_t TmpfsFilesystem::allocate_inode() {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (config_.max_inodes > 0 && used_inodes_ >= config_.max_inodes) {
        LOG_WARN("tmpfs: Out of inodes (%lu/%lu)", used_inodes_, config_.max_inodes);
        return 0;
    }

    used_inodes_++;
    return next_inode_++;
}

void TmpfsFilesystem::free_inode() {
    std::lock_guard<std::mutex> lock(fs_mutex_);
    if (used_inodes_ > 0) {
        used_inodes_--;
    }
}

bool TmpfsFilesystem::allocate_space(size_t bytes) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (config_.max_size > 0 && used_bytes_ + bytes > config_.max_size) {
        LOG_WARN("tmpfs: Out of space (%lu + %zu > %lu)",
                 used_bytes_, bytes, config_.max_size);
        return false;
    }

    used_bytes_ += bytes;
    return true;
}

void TmpfsFilesystem::free_space(size_t bytes) {
    std::lock_guard<std::mutex> lock(fs_mutex_);
    if (used_bytes_ >= bytes) {
        used_bytes_ -= bytes;
    } else {
        used_bytes_ = 0;
    }
}

std::shared_ptr<TmpfsNode> TmpfsFilesystem::create_node(FileType type, uint32_t mode) {
    uint64_t inode = allocate_inode();
    if (inode == 0) {
        return nullptr;
    }

    auto node = std::make_shared<TmpfsNode>(this, type, inode, mode);
    return node;
}

} // namespace xinim::vfs

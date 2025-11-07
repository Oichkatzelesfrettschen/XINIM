/**
 * @file vfs.cpp
 * @brief Virtual File System implementation
 * 
 * Provides Unix-style filesystem abstraction with VNode interface.
 * Based on BSD and Linux VFS designs.
 */

#include <xinim/vfs/vfs.hpp>
#include <algorithm>
#include <cstring>

namespace xinim::vfs {

// VNode implementation

VNode::VNode(FileType type, uint32_t mode)
    : type_(type)
    , mode_(mode)
    , size_(0)
    , links_(1)
{
}

VNode::~VNode() = default;

int VNode::read(void* buffer, size_t size, uint64_t offset) {
    return -ENOTSUP;  // Override in subclasses
}

int VNode::write(const void* buffer, size_t size, uint64_t offset) {
    return -ENOTSUP;  // Override in subclasses
}

int VNode::truncate(uint64_t new_size) {
    return -ENOTSUP;  // Override in subclasses
}

int VNode::sync() {
    return 0;  // Override if needed
}

std::vector<std::string> VNode::readdir() {
    return {};  // Override for directories
}

VNode* VNode::lookup(const std::string& name) {
    return nullptr;  // Override for directories
}

int VNode::create(const std::string& name, uint32_t mode, VNode** out) {
    return -ENOTSUP;  // Override for directories
}

int VNode::mkdir(const std::string& name, uint32_t mode, VNode** out) {
    return -ENOTSUP;  // Override for directories
}

int VNode::unlink(const std::string& name) {
    return -ENOTSUP;  // Override for directories
}

int VNode::rmdir(const std::string& name) {
    return -ENOTSUP;  // Override for directories
}

int VNode::link(const std::string& name, VNode* target) {
    return -ENOTSUP;  // Override for directories
}

int VNode::symlink(const std::string& name, const std::string& target) {
    return -ENOTSUP;  // Override for directories
}

int VNode::rename(const std::string& old_name, VNode* new_dir, const std::string& new_name) {
    return -ENOTSUP;  // Override for directories
}

std::string VNode::readlink() {
    return "";  // Override for symlinks
}

FileAttributes VNode::get_attributes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    FileAttributes attrs;
    attrs.mode = mode_;
    attrs.size = size_;
    attrs.links = links_;
    attrs.uid = 0;    // TODO: Implement
    attrs.gid = 0;    // TODO: Implement
    attrs.atime = 0;  // TODO: Implement
    attrs.mtime = 0;  // TODO: Implement
    attrs.ctime = 0;  // TODO: Implement
    
    return attrs;
}

int VNode::set_attributes(const FileAttributes& attrs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Only allow changing certain attributes
    mode_ = attrs.mode;
    // TODO: Implement uid/gid/times
    
    return 0;
}

// FileSystem implementation

FileSystem::~FileSystem() = default;

// VFS implementation

VFS::VFS()
    : next_fd_(3)  // 0, 1, 2 are stdin/stdout/stderr
{
}

VFS& VFS::instance() {
    static VFS vfs;
    return vfs;
}

int VFS::register_filesystem(const std::string& name, FileSystemFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (filesystems_.count(name)) {
        return -EEXIST;
    }
    
    filesystems_[name] = factory;
    return 0;
}

int VFS::mount(const std::string& device, const std::string& path, 
               const std::string& fstype, uint32_t flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if filesystem type is registered
    auto fs_it = filesystems_.find(fstype);
    if (fs_it == filesystems_.end()) {
        return -ENODEV;
    }
    
    // Create filesystem instance
    auto fs = fs_it->second();
    if (!fs) {
        return -ENOMEM;
    }
    
    // Mount the filesystem
    int ret = fs->mount(device, flags);
    if (ret < 0) {
        return ret;
    }
    
    // Get root vnode
    VNode* root = fs->get_root();
    if (!root) {
        return -EINVAL;
    }
    
    // Add mount point
    MountPoint mp;
    mp.path = path;
    mp.device = device;
    mp.fstype = fstype;
    mp.flags = flags;
    mp.fs = std::move(fs);
    mp.root = root;
    
    mounts_[path] = std::move(mp);
    
    return 0;
}

int VFS::unmount(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = mounts_.find(path);
    if (it == mounts_.end()) {
        return -EINVAL;
    }
    
    // TODO: Check if filesystem is busy
    
    // Sync filesystem
    it->second.fs->sync();
    
    // Unmount
    it->second.fs->unmount();
    
    // Remove mount point
    mounts_.erase(it);
    
    return 0;
}

VNode* VFS::open(const std::string& path, uint32_t flags, uint32_t mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    VNode* vnode = resolve_path(path, true);
    if (!vnode) {
        // File doesn't exist
        if (!(flags & O_CREAT)) {
            return nullptr;
        }
        
        // Create new file
        // TODO: Implement file creation
        return nullptr;
    }
    
    // TODO: Check permissions
    
    return vnode;
}

int VFS::close(VNode* vnode) {
    if (!vnode) {
        return -EINVAL;
    }
    
    // TODO: Implement reference counting and cleanup
    
    return 0;
}

VNode* VFS::resolve_path(const std::string& path, bool follow_symlinks) {
    if (path.empty() || path[0] != '/') {
        return nullptr;  // Only absolute paths for now
    }
    
    // Find the mount point
    std::string mount_path = "/";
    VNode* root = nullptr;
    
    for (const auto& [mp_path, mp] : mounts_) {
        if (path.starts_with(mp_path) && mp_path.size() > mount_path.size()) {
            mount_path = mp_path;
            root = mp.root;
        }
    }
    
    if (!root) {
        return nullptr;  // No mount point found
    }
    
    // Remove mount point prefix
    std::string rel_path = path.substr(mount_path.size());
    if (rel_path.empty() || rel_path[0] != '/') {
        rel_path = "/" + rel_path;
    }
    
    // Traverse path components
    VNode* current = root;
    std::string component;
    size_t pos = 1;  // Skip leading /
    
    while (pos < rel_path.size()) {
        size_t next = rel_path.find('/', pos);
        if (next == std::string::npos) {
            next = rel_path.size();
        }
        
        component = rel_path.substr(pos, next - pos);
        pos = next + 1;
        
        if (component.empty() || component == ".") {
            continue;
        }
        
        if (component == "..") {
            // TODO: Implement parent directory lookup
            continue;
        }
        
        // Lookup component
        VNode* next_node = current->lookup(component);
        if (!next_node) {
            return nullptr;
        }
        
        // Handle symlinks
        if (follow_symlinks && next_node->type() == FileType::Symlink) {
            std::string target = next_node->readlink();
            // TODO: Recursively resolve symlink
            return nullptr;  // Not yet implemented
        }
        
        current = next_node;
    }
    
    return current;
}

int VFS::allocate_fd(VNode* vnode) {
    std::lock_guard<std::mutex> lock(fd_mutex_);
    
    int fd = next_fd_++;
    fds_[fd] = vnode;
    
    return fd;
}

VNode* VFS::get_vnode(int fd) {
    std::lock_guard<std::mutex> lock(fd_mutex_);
    
    auto it = fds_.find(fd);
    if (it == fds_.end()) {
        return nullptr;
    }
    
    return it->second;
}

void VFS::release_fd(int fd) {
    std::lock_guard<std::mutex> lock(fd_mutex_);
    
    fds_.erase(fd);
}

} // namespace xinim::vfs

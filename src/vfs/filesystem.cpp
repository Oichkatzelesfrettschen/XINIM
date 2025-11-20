/**
 * @file filesystem.cpp
 * @brief Filesystem Driver Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/filesystem.hpp>
#include <xinim/vfs/buffer_cache.hpp>
#include <xinim/block/blockdev.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <algorithm>

// Error codes
#define EINVAL 22  // Invalid argument
#define EIO    5   // I/O error
#define ENODEV 19  // No such device
#define EBUSY  16  // Device or resource busy
#define EROFS  30  // Read-only filesystem
#define EEXIST 17  // File exists

namespace xinim::vfs {

// ============================================================================
// Global Buffer Cache I/O Callbacks
// ============================================================================

namespace {
    // Map device name to device pointer
    std::unordered_map<uint64_t, std::shared_ptr<block::BlockDevice>> device_map_;
    std::mutex device_map_mutex_;

    // Callback for buffer cache to read from device
    int cache_read_callback(uint64_t device_id, uint64_t block_num,
                           uint32_t block_size, void* buffer) {
        std::lock_guard<std::mutex> lock(device_map_mutex_);

        auto it = device_map_.find(device_id);
        if (it == device_map_.end()) {
            return -ENODEV;
        }

        auto device = it->second;
        if (!device) {
            return -ENODEV;
        }

        // Read from device
        size_t dev_block_size = device->get_block_size();
        uint64_t blocks_per_fs_block = block_size / dev_block_size;
        uint64_t dev_block = block_num * blocks_per_fs_block;

        return device->read_blocks(dev_block, blocks_per_fs_block,
                                  static_cast<uint8_t*>(buffer));
    }

    // Callback for buffer cache to write to device
    int cache_write_callback(uint64_t device_id, uint64_t block_num,
                            uint32_t block_size, const void* buffer) {
        std::lock_guard<std::mutex> lock(device_map_mutex_);

        auto it = device_map_.find(device_id);
        if (it == device_map_.end()) {
            return -ENODEV;
        }

        auto device = it->second;
        if (!device) {
            return -ENODEV;
        }

        // Write to device
        size_t dev_block_size = device->get_block_size();
        uint64_t blocks_per_fs_block = block_size / dev_block_size;
        uint64_t dev_block = block_num * blocks_per_fs_block;

        return device->write_blocks(dev_block, blocks_per_fs_block,
                                   static_cast<const uint8_t*>(buffer));
    }

    // Initialize buffer cache callbacks (once)
    bool init_buffer_cache() {
        static bool initialized = false;
        if (!initialized) {
            auto& cache = get_global_buffer_cache();
            cache.set_read_callback(cache_read_callback);
            cache.set_write_callback(cache_write_callback);
            initialized = true;
            LOG_INFO("FS: Buffer cache initialized with I/O callbacks");
        }
        return true;
    }
}  // anonymous namespace

// ============================================================================
// MountOptions Implementation
// ============================================================================

MountOptions MountOptions::parse(const std::string& options) {
    MountOptions opts;

    if (options.empty()) {
        return opts;
    }

    // Split by commas
    size_t start = 0;
    size_t end = options.find(',');

    while (start < options.length()) {
        std::string opt = options.substr(start, end - start);

        if (opt == "ro" || opt == "r") {
            opts.read_only = true;
        } else if (opt == "rw") {
            opts.read_only = false;
        } else if (opt == "noexec") {
            opts.no_exec = true;
        } else if (opt == "nosuid") {
            opts.no_suid = true;
        } else if (opt == "nodev") {
            opts.no_dev = true;
        } else if (opt == "sync") {
            opts.synchronous = true;
        } else if (opt == "noatime") {
            opts.no_atime = true;
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
        end = options.find(',', start);
        if (end == std::string::npos) {
            end = options.length();
        }
    }

    return opts;
}

uint32_t MountOptions::to_flags() const {
    uint32_t flags = 0;

    if (read_only)    flags |= MountPoint::RDONLY;
    if (no_exec)      flags |= MountPoint::NOEXEC;
    if (no_suid)      flags |= MountPoint::NOSUID;
    if (no_dev)       flags |= MountPoint::NODEV;

    return flags;
}

MountOptions MountOptions::from_flags(uint32_t flags) {
    MountOptions opts;

    opts.read_only = (flags & MountPoint::RDONLY) != 0;
    opts.no_exec   = (flags & MountPoint::NOEXEC) != 0;
    opts.no_suid   = (flags & MountPoint::NOSUID) != 0;
    opts.no_dev    = (flags & MountPoint::NODEV) != 0;

    return opts;
}

// ============================================================================
// BlockFilesystem Implementation
// ============================================================================

BlockFilesystem::BlockFilesystem()
    : block_device_(nullptr),
      root_vnode_(nullptr),
      mounted_(false),
      block_size_(0),
      total_blocks_(0),
      free_blocks_(0),
      total_inodes_(0),
      free_inodes_(0) {
}

BlockFilesystem::~BlockFilesystem() {
    if (mounted_) {
        unmount();
    }
}

int BlockFilesystem::mount(const std::string& device, uint32_t flags) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (mounted_) {
        return -EBUSY;
    }

    // Initialize buffer cache (one-time setup)
    init_buffer_cache();

    // Parse mount options
    mount_options_ = MountOptions::from_flags(flags);
    device_name_ = device;

    // Get block device from BlockDeviceManager
    auto& mgr = block::BlockDeviceManager::instance();
    block_device_ = mgr.get_device(device);

    if (!block_device_) {
        LOG_ERROR("FS: Block device %s not found", device.c_str());
        return -ENODEV;
    }

    // Register device in device map for buffer cache callbacks
    {
        std::lock_guard<std::mutex> dev_lock(device_map_mutex_);
        uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
        device_map_[device_id] = block_device_;
    }

    LOG_INFO("FS: Mounting %s on %s (%s)", device.c_str(), get_type().c_str(),
             mount_options_.read_only ? "ro" : "rw");

    // Read superblock (implemented by subclass)
    int ret = read_superblock();
    if (ret < 0) {
        LOG_ERROR("FS: Failed to read superblock from %s: %d", device.c_str(), ret);

        // Unregister device
        {
            std::lock_guard<std::mutex> dev_lock(device_map_mutex_);
            uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
            device_map_.erase(device_id);
        }

        block_device_ = nullptr;
        return ret;
    }

    // Create root vnode (implemented by subclass)
    root_vnode_ = create_root_vnode();
    if (!root_vnode_) {
        LOG_ERROR("FS: Failed to create root vnode for %s", device.c_str());

        // Unregister device
        {
            std::lock_guard<std::mutex> dev_lock(device_map_mutex_);
            uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
            device_map_.erase(device_id);
        }

        block_device_ = nullptr;
        return -EIO;
    }

    mounted_ = true;
    LOG_INFO("FS: Successfully mounted %s", device.c_str());
    return 0;
}

int BlockFilesystem::unmount() {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (!mounted_) {
        return -EINVAL;
    }

    LOG_INFO("FS: Unmounting %s", device_name_.c_str());

    // Sync all data to disk
    int ret = sync();
    if (ret < 0) {
        LOG_WARN("FS: Sync failed during unmount: %d", ret);
    }

    // Invalidate all cached blocks for this device
    if (block_device_) {
        uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
        auto& cache = get_global_buffer_cache();
        cache.invalidate_device(device_id);

        // Unregister device from device map
        {
            std::lock_guard<std::mutex> dev_lock(device_map_mutex_);
            device_map_.erase(device_id);
        }
    }

    // Clean up root vnode
    if (root_vnode_) {
        // TODO: Implement proper vnode cleanup
        root_vnode_ = nullptr;
    }

    // Release block device
    block_device_ = nullptr;
    mounted_ = false;

    LOG_INFO("FS: Successfully unmounted %s", device_name_.c_str());
    return 0;
}

VNode* BlockFilesystem::get_root() {
    std::lock_guard<std::mutex> lock(fs_mutex_);
    return root_vnode_;
}

int BlockFilesystem::sync() {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (!mounted_) {
        return -EINVAL;
    }

    if (mount_options_.read_only) {
        return 0;  // Nothing to sync on read-only filesystem
    }

    // Flush buffer cache for this device
    if (block_device_) {
        uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
        auto& cache = get_global_buffer_cache();
        int ret = cache.sync_device(device_id);
        if (ret < 0) {
            LOG_ERROR("FS: Failed to flush buffer cache: %d", ret);
            return ret;
        }
    }

    // Write superblock (implemented by subclass)
    int ret = write_superblock();
    if (ret < 0) {
        LOG_ERROR("FS: Failed to write superblock: %d", ret);
        return ret;
    }

    // Flush all dirty data (implemented by subclass)
    ret = flush_all();
    if (ret < 0) {
        LOG_ERROR("FS: Failed to flush data: %d", ret);
        return ret;
    }

    // Flush block device cache
    if (block_device_) {
        block_device_->flush();
    }

    return 0;
}

FilesystemStats BlockFilesystem::get_stats() const {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    FilesystemStats stats = {};
    stats.block_size = block_size_;
    stats.total_blocks = total_blocks_;
    stats.free_blocks = free_blocks_;
    stats.available_blocks = free_blocks_;  // TODO: Reserve for root
    stats.total_inodes = total_inodes_;
    stats.free_inodes = free_inodes_;
    stats.available_inodes = free_inodes_;  // TODO: Reserve for root
    stats.max_filename_len = 255;  // Default

    return stats;
}

int BlockFilesystem::read_block(uint64_t block_num, void* buffer) {
    if (!block_device_) {
        return -ENODEV;
    }

    // Use buffer cache for all reads
    uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
    auto& cache = get_global_buffer_cache();

    return cache.get_block(device_id, block_num, block_size_, buffer);
}

int BlockFilesystem::write_block(uint64_t block_num, const void* buffer) {
    if (!block_device_) {
        return -ENODEV;
    }

    if (mount_options_.read_only) {
        return -EROFS;
    }

    // Use buffer cache for all writes
    uint64_t device_id = reinterpret_cast<uint64_t>(block_device_.get());
    auto& cache = get_global_buffer_cache();

    return cache.put_block(device_id, block_num, block_size_, buffer);
}

int BlockFilesystem::read_blocks(uint64_t start_block, uint32_t count, void* buffer) {
    for (uint32_t i = 0; i < count; i++) {
        int ret = read_block(start_block + i,
                           static_cast<uint8_t*>(buffer) + (i * block_size_));
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int BlockFilesystem::write_blocks(uint64_t start_block, uint32_t count, const void* buffer) {
    for (uint32_t i = 0; i < count; i++) {
        int ret = write_block(start_block + i,
                            static_cast<const uint8_t*>(buffer) + (i * block_size_));
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

// ============================================================================
// PseudoFilesystem Implementation
// ============================================================================

PseudoFilesystem::PseudoFilesystem()
    : root_vnode_(nullptr),
      mounted_(false) {
}

PseudoFilesystem::~PseudoFilesystem() {
    if (mounted_) {
        unmount();
    }
}

int PseudoFilesystem::mount(const std::string& device, uint32_t flags) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (mounted_) {
        return -EBUSY;
    }

    LOG_INFO("FS: Mounting pseudo filesystem %s", get_type().c_str());

    // Initialize filesystem (implemented by subclass)
    int ret = initialize();
    if (ret < 0) {
        LOG_ERROR("FS: Failed to initialize %s: %d", get_type().c_str(), ret);
        return ret;
    }

    mounted_ = true;
    LOG_INFO("FS: Successfully mounted %s", get_type().c_str());
    return 0;
}

int PseudoFilesystem::unmount() {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (!mounted_) {
        return -EINVAL;
    }

    LOG_INFO("FS: Unmounting pseudo filesystem %s", get_type().c_str());

    // Clean up root vnode
    if (root_vnode_) {
        // TODO: Implement proper vnode cleanup
        root_vnode_ = nullptr;
    }

    mounted_ = false;
    return 0;
}

VNode* PseudoFilesystem::get_root() {
    std::lock_guard<std::mutex> lock(fs_mutex_);
    return root_vnode_;
}

int PseudoFilesystem::sync() {
    // Pseudo filesystems are in-memory, nothing to sync
    return 0;
}

// ============================================================================
// FilesystemRegistry Implementation
// ============================================================================

FilesystemRegistry& FilesystemRegistry::instance() {
    static FilesystemRegistry instance;
    return instance;
}

int FilesystemRegistry::register_filesystem(const std::string& type, FilesystemFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (factories_.find(type) != factories_.end()) {
        LOG_WARN("FS: Filesystem type %s already registered", type.c_str());
        return -EEXIST;
    }

    factories_[type] = factory;
    LOG_INFO("FS: Registered filesystem type: %s", type.c_str());
    return 0;
}

int FilesystemRegistry::unregister_filesystem(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = factories_.find(type);
    if (it == factories_.end()) {
        return -EINVAL;
    }

    factories_.erase(it);
    LOG_INFO("FS: Unregistered filesystem type: %s", type.c_str());
    return 0;
}

std::unique_ptr<FileSystem> FilesystemRegistry::create_filesystem(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = factories_.find(type);
    if (it == factories_.end()) {
        LOG_ERROR("FS: Unknown filesystem type: %s", type.c_str());
        return nullptr;
    }

    return it->second();
}

std::vector<std::string> FilesystemRegistry::get_registered_types() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> types;
    types.reserve(factories_.size());

    for (const auto& [type, factory] : factories_) {
        types.push_back(type);
    }

    std::sort(types.begin(), types.end());
    return types;
}

} // namespace xinim::vfs

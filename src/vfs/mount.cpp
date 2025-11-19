/**
 * @file mount.cpp
 * @brief Mount Table Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/mount.hpp>
#include <xinim/vfs/filesystem.hpp>
#include <xinim/log.hpp>
#include <algorithm>
#include <cstring>

// Error codes
#define EINVAL 22  // Invalid argument
#define ENODEV 19  // No such device
#define EBUSY  16  // Device or resource busy
#define ENOENT 2   // No such file or directory
#define EEXIST 17  // File exists

namespace xinim::vfs {

MountTable::MountTable()
    : next_mount_id_(1) {
}

MountTable::~MountTable() {
    // Unmount all filesystems
    std::lock_guard<std::mutex> lock(mutex_);
    mounts_.clear();
}

MountTable& MountTable::instance() {
    static MountTable instance;
    return instance;
}

int MountTable::mount(const std::string& device,
                      const std::string& mount_point,
                      const std::string& filesystem_type,
                      const std::string& options) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOG_INFO("Mount: Mounting %s at %s (type: %s, options: %s)",
             device.c_str(), mount_point.c_str(),
             filesystem_type.c_str(), options.empty() ? "defaults" : options.c_str());

    // Validate mount point
    int ret = validate_mount_point(mount_point);
    if (ret < 0) {
        LOG_ERROR("Mount: Invalid mount point: %s", mount_point.c_str());
        return ret;
    }

    // Check if already mounted
    if (mounts_.find(mount_point) != mounts_.end()) {
        LOG_ERROR("Mount: %s is already a mount point", mount_point.c_str());
        return -EBUSY;
    }

    // Parse mount options
    MountOptions mount_opts = MountOptions::parse(options);

    // Get filesystem from registry
    auto& registry = FilesystemRegistry::instance();
    auto filesystem = registry.create_filesystem(filesystem_type);
    if (!filesystem) {
        LOG_ERROR("Mount: Unknown filesystem type: %s", filesystem_type.c_str());
        return -ENODEV;
    }

    // Mount the filesystem
    ret = filesystem->mount(device, mount_opts.to_flags());
    if (ret < 0) {
        LOG_ERROR("Mount: Failed to mount %s: %d", device.c_str(), ret);
        return ret;
    }

    // Get root vnode
    VNode* root = filesystem->get_root();
    if (!root) {
        LOG_ERROR("Mount: Failed to get root vnode for %s", device.c_str());
        filesystem->unmount();
        return -EINVAL;
    }

    // Create mount info
    auto mount_info = std::make_unique<MountInfo>();
    mount_info->mount_point = mount_point;
    mount_info->device = device;
    mount_info->filesystem_type = filesystem_type;
    mount_info->options = mount_opts;
    mount_info->filesystem = std::move(filesystem);
    mount_info->root_vnode = root;
    mount_info->mount_time = 0;  // TODO: Get current timestamp
    mount_info->mount_id = next_mount_id_++;

    // Add to mount table
    mounts_[mount_point] = std::move(mount_info);

    LOG_INFO("Mount: Successfully mounted %s at %s (mount ID: %u)",
             device.c_str(), mount_point.c_str(), next_mount_id_ - 1);

    return 0;
}

int MountTable::unmount(const std::string& mount_point, bool force) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOG_INFO("Mount: Unmounting %s%s", mount_point.c_str(), force ? " (forced)" : "");

    // Find mount point
    auto it = mounts_.find(mount_point);
    if (it == mounts_.end()) {
        LOG_ERROR("Mount: %s is not a mount point", mount_point.c_str());
        return -EINVAL;
    }

    // Check if filesystem is busy
    if (!force && is_filesystem_busy(mount_point)) {
        LOG_ERROR("Mount: %s is busy", mount_point.c_str());
        return -EBUSY;
    }

    // Sync filesystem
    if (it->second->filesystem) {
        int ret = it->second->filesystem->sync();
        if (ret < 0 && !force) {
            LOG_ERROR("Mount: Failed to sync %s: %d", mount_point.c_str(), ret);
            return ret;
        }
    }

    // Unmount filesystem
    if (it->second->filesystem) {
        it->second->filesystem->unmount();
    }

    // Remove from mount table
    mounts_.erase(it);

    LOG_INFO("Mount: Successfully unmounted %s", mount_point.c_str());
    return 0;
}

bool MountTable::is_mount_point(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mounts_.find(path) != mounts_.end();
}

const MountInfo* MountTable::get_mount_info(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = mounts_.find(path);
    if (it != mounts_.end()) {
        return it->second.get();
    }

    return nullptr;
}

std::vector<std::string> MountTable::get_mount_points() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> mount_points;
    mount_points.reserve(mounts_.size());

    for (const auto& [path, info] : mounts_) {
        mount_points.push_back(path);
    }

    std::sort(mount_points.begin(), mount_points.end());
    return mount_points;
}

std::string MountTable::find_mount_point(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find the longest matching mount point
    std::string best_match = "/";
    size_t best_len = 1;

    for (const auto& [mount_point, info] : mounts_) {
        // Check if path starts with mount_point
        if (path.find(mount_point) == 0) {
            // Check for exact match or path separator
            if (path.length() == mount_point.length() ||
                (path.length() > mount_point.length() && path[mount_point.length()] == '/')) {
                if (mount_point.length() > best_len) {
                    best_match = mount_point;
                    best_len = mount_point.length();
                }
            }
        }
    }

    return best_match;
}

VNode* MountTable::get_root_vnode(const std::string& mount_point) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = mounts_.find(mount_point);
    if (it != mounts_.end()) {
        return it->second->root_vnode;
    }

    return nullptr;
}

void MountTable::print_mounts() const {
    std::lock_guard<std::mutex> lock(mutex_);

    printf("\n");
    printf("=== Mounted Filesystems ===\n");
    printf("\n");
    printf("%-20s %-15s %-10s %-20s\n", "Mount Point", "Device", "Type", "Options");
    printf("%-20s %-15s %-10s %-20s\n", "-----------", "------", "----", "-------");

    std::vector<std::string> sorted_mounts;
    for (const auto& [mount_point, info] : mounts_) {
        sorted_mounts.push_back(mount_point);
    }
    std::sort(sorted_mounts.begin(), sorted_mounts.end());

    for (const auto& mount_point : sorted_mounts) {
        const auto& info = mounts_.at(mount_point);

        std::string opts_str;
        if (info->options.read_only) opts_str += "ro,";
        else opts_str += "rw,";
        if (info->options.no_exec) opts_str += "noexec,";
        if (info->options.no_suid) opts_str += "nosuid,";
        if (info->options.no_dev) opts_str += "nodev,";
        if (info->options.no_atime) opts_str += "noatime,";
        if (!opts_str.empty() && opts_str.back() == ',') {
            opts_str.pop_back();
        }
        if (opts_str.empty()) opts_str = "defaults";

        printf("%-20s %-15s %-10s %-20s\n",
               mount_point.c_str(),
               info->device.c_str(),
               info->filesystem_type.c_str(),
               opts_str.c_str());
    }

    printf("\n");
    printf("Total mounts: %zu\n", mounts_.size());
    printf("\n");
}

int MountTable::sync_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    LOG_INFO("Mount: Syncing all filesystems...");

    int errors = 0;
    for (const auto& [mount_point, info] : mounts_) {
        if (info->filesystem) {
            int ret = info->filesystem->sync();
            if (ret < 0) {
                LOG_ERROR("Mount: Failed to sync %s: %d", mount_point.c_str(), ret);
                errors++;
            }
        }
    }

    if (errors > 0) {
        LOG_WARN("Mount: Sync completed with %d errors", errors);
        return -1;
    }

    LOG_INFO("Mount: All filesystems synced successfully");
    return 0;
}

int MountTable::validate_mount_point(const std::string& path) const {
    // Must be absolute path
    if (path.empty() || path[0] != '/') {
        return -EINVAL;
    }

    // TODO: Check if path exists (once we have filesystem operations)
    // TODO: Check if path is a directory

    return 0;
}

bool MountTable::is_filesystem_busy(const std::string& mount_point) const {
    // TODO: Implement proper busy check
    // - Check for open files
    // - Check for processes with working directory in this filesystem
    // - Check for other mount points below this one

    return false;  // For now, assume not busy
}

} // namespace xinim::vfs

/**
 * @file fs_init.cpp
 * @brief Filesystem Initialization Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/fs_init.hpp>
#include <xinim/vfs/filesystem.hpp>
#include <xinim/vfs/tmpfs.hpp>
#include <xinim/log.hpp>
#include <memory>

namespace xinim::vfs {

int initialize_filesystems() {
    auto& registry = FilesystemRegistry::instance();
    int errors = 0;

    LOG_INFO("VFS: Initializing built-in filesystems...");

    // Register tmpfs
    int ret = registry.register_filesystem("tmpfs", []() -> std::unique_ptr<FileSystem> {
        return std::make_unique<TmpfsFilesystem>();
    });
    if (ret < 0) {
        LOG_ERROR("VFS: Failed to register tmpfs: %d", ret);
        errors++;
    } else {
        LOG_INFO("VFS: Registered tmpfs");
    }

    // TODO: Register other filesystems (ext2, fat32, etc.) when implemented

    if (errors > 0) {
        LOG_ERROR("VFS: Filesystem initialization completed with %d errors", errors);
        return -1;
    }

    LOG_INFO("VFS: Filesystem initialization complete");
    return 0;
}

} // namespace xinim::vfs

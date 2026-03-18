#pragma once
// Virtual Filesystem interface for the i486 kernel.
// Routes path-based operations through a mount table to filesystem backends.

#include <stdint.h>
#include "xinim/userland/userspace_stat.hpp"

namespace xinim::i486::vfs {

using UserspaceStat = ::xinim::userland::UserspaceStat;

struct VfsOps {
    int (*open)(const char* path, uint32_t flags, uint32_t mode);
    int (*read)(int slot, void* buf, uint32_t count);
    int (*write)(int slot, const void* buf, uint32_t count);
    int (*close)(int slot);
    int (*stat)(const char* path, UserspaceStat* buf);
    int (*mkdir)(const char* path, uint32_t mode);
    int (*unlink)(const char* path);
    int (*rename)(const char* old_path, const char* new_path);
    int (*readdir)(int slot, void* buf, uint32_t count);
    int (*chmod)(const char* path, uint16_t mode);
    int (*symlink)(const char* target, const char* linkpath);
    int (*readlink)(const char* path, char* buf, uint32_t size);
    int (*rmdir)(const char* path);
    int (*access)(const char* path);
    int (*truncate)(const char* path, uint32_t size);
};

constexpr int kMaxMounts = 8;

struct MountEntry {
    bool in_use;
    char prefix[64];
    uint32_t prefix_len;
    VfsOps* ops;
};

// Initialize the VFS mount table.
void initialize() noexcept;

// Mount a filesystem at a given prefix. Returns true on success.
bool mount(const char* prefix, VfsOps* ops) noexcept;

// Find the VfsOps for a given path. Returns null if no mount matches.
// Sets relative_path to point past the mount prefix.
VfsOps* resolve(const char* path, const char** relative_path) noexcept;

// Get the mount table entry for a path (for stat to know device ID).
int mount_index(const char* path) noexcept;

} // namespace xinim::i486::vfs

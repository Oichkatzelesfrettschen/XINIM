/**
 * @file mount_table.cpp
 * @brief Mount table for bare-metal VFS (ADR-0009)
 *
 * Fixed array of MAX_MOUNTS=8 entries. Longest-prefix matching done
 * with O(MAX_MOUNTS) linear scan -- adequate for 8 mount points.
 *
 * Initialisation pre-mounts "/" at ino=1 with ramfs_ops.
 */

#include "bare_vfs.hpp"
#include "ramfs_ops.hpp"
#include "inode_table.hpp"
#include <cstring>

// ============================================================================
// Global storage
// ============================================================================

static MountEntry g_mounts[MAX_MOUNTS];

// ============================================================================
// Helpers
// ============================================================================

static inline uint32_t cstr_len(const char* s, uint32_t max) {
    uint32_t n = 0;
    while (n < max && s[n] != '\0') {
        ++n;
    }
    return n;
}

// Returns the number of matching characters between path and mount->path,
// but only if mount->path is a proper prefix of path.
//
// Special case: mpath="/" matches any absolute path (all paths start with '/').
// For non-root mounts: mpath must end at a '/' boundary in path.
static uint32_t prefix_match_len(const char* path, const char* mpath) {
    uint32_t i = 0;
    while (mpath[i] != '\0' && path[i] == mpath[i]) {
        ++i;
    }
    if (mpath[i] != '\0') {
        return 0; // mpath is not a prefix of path
    }

    // Root mount "/" always matches absolute paths (path[0]=='/')
    if (i == 1 && mpath[0] == '/') {
        return 1;
    }

    // For non-root mounts: path[i] must be '/' or '\0' to avoid partial
    // component matches (e.g., mpath="/foo" should not match "/foobar")
    if (path[i] != '\0' && path[i] != '/') {
        return 0;
    }
    return i;
}

// ============================================================================
// Public API
// ============================================================================

void mount_table_init() {
    __builtin_memset(g_mounts, 0, sizeof(g_mounts));

    // Pre-mount "/" at root inode (ino=1) with ramfs_ops.
    // Root inode is created by vfs_server_init() before this call.
    g_mounts[0].root_ino = 1;
    g_mounts[0].ops      = &ramfs_ops;
    g_mounts[0].path[0]  = '/';
    g_mounts[0].path[1]  = '\0';
}

// Mount a filesystem at path with given root inode and FsOps.
// Returns 0 on success, -1 if table full or path too long.
int mount_add(const char* path, uint32_t root_ino, const FsOps* ops) {
    if (!path || !ops || root_ino == 0) {
        return -1;
    }
    uint32_t plen = cstr_len(path, 47);
    if (plen == 0 || plen > 47) {
        return -1;
    }

    for (uint32_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mounts[i].root_ino == 0) {
            g_mounts[i].root_ino = root_ino;
            g_mounts[i].ops      = ops;
            __builtin_memcpy(g_mounts[i].path, path, plen + 1);
            return 0;
        }
    }
    return -1; // full
}

// Remove mount at path. Returns 0 on success, -1 if not found.
int mount_remove(const char* path) {
    if (!path) {
        return -1;
    }
    uint32_t plen = cstr_len(path, 47);
    for (uint32_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mounts[i].root_ino == 0) {
            continue;
        }
        if (__builtin_memcmp(g_mounts[i].path, path, plen + 1) == 0) {
            __builtin_memset(&g_mounts[i], 0, sizeof(MountEntry));
            return 0;
        }
    }
    return -1;
}

// Longest-prefix resolve: return the MountEntry whose path is the longest
// prefix of the given path. Returns nullptr if no match (should not happen
// if "/" is always mounted).
const MountEntry* mount_resolve(const char* path) {
    if (!path) {
        return nullptr;
    }
    const MountEntry* best = nullptr;
    uint32_t          best_len = 0;
    for (uint32_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mounts[i].root_ino == 0) {
            continue;
        }
        uint32_t mlen = prefix_match_len(path, g_mounts[i].path);
        if (mlen > best_len) {
            best_len = mlen;
            best     = &g_mounts[i];
        }
    }
    return best;
}

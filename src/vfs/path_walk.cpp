/**
 * @file path_walk.cpp
 * @brief Zero-allocation path resolution for bare-metal VFS (ADR-0009)
 *
 * Components are sliced in-place from the original path buffer using
 * two pointers. No heap allocation. No std::string.
 *
 * "." components are skipped. ".." resolves to parent_ino stored in
 * the inode (set at mkdir time by ramfs_ops).
 *
 * Root inode is ino=1 (initialised by vfs_server_init).
 */

#include "path_walk.hpp"
#include "inode_table.hpp"
#include "dirent.hpp"

inline constexpr uint32_t ROOT_INO = 1;

// ============================================================================
// Internal: advance p past '/' chars, return start; fill len with component
// length up to next '/' or '\0'.
// ============================================================================

static const char* next_component(const char* p, uint8_t* out_len) {
    // Skip leading slashes
    while (*p == '/') ++p;
    const char* start = p;
    uint8_t len = 0;
    while (*p != '\0' && *p != '/') {
        ++p;
        if (len < 255) ++len;
    }
    *out_len = len;
    return start;
}

// ============================================================================
// Public API
// ============================================================================

uint32_t path_walk(const char* path) {
    if (!path || path[0] != '/') return 0; // must be absolute

    uint32_t cur_ino = ROOT_INO;
    const char* p = path;

    for (;;) {
        uint8_t     len;
        const char* comp = next_component(p, &len);
        p = comp + len; // advance past component

        if (len == 0) {
            // Trailing slash or end of path -- we've arrived
            return cur_ino;
        }

        // "." -- stay in current directory
        if (len == 1 && comp[0] == '.') continue;

        // ".." -- go to parent
        if (len == 2 && comp[0] == '.' && comp[1] == '.') {
            RawInode* inode = inode_get(cur_ino);
            if (!inode) return 0;
            // Root's parent is itself
            cur_ino = (inode->parent_ino != 0) ? inode->parent_ino : ROOT_INO;
            continue;
        }

        if (len > 26) return 0; // name too long for this table

        uint32_t child = dirent_lookup(cur_ino, comp, len);
        if (child == 0) return 0; // not found
        cur_ino = child;
    }
}

uint32_t path_walk_parent(const char* path,
                          const char** out_name,
                          uint8_t*     out_namelen) {
    if (!path || path[0] != '/') return 0;
    if (!out_name || !out_namelen) return 0;

    // Pass 1: collect all non-empty components into a small component list.
    // We support up to 32 path components (adequate for any kernel path).
    constexpr int MAX_COMPONENTS = 32;
    const char* comp_start[MAX_COMPONENTS];
    uint8_t     comp_len[MAX_COMPONENTS];
    int         ncomps = 0;

    const char* p = path;
    for (;;) {
        uint8_t     len;
        const char* comp = next_component(p, &len);
        p = comp + len;
        if (len == 0) break;
        if (ncomps < MAX_COMPONENTS) {
            comp_start[ncomps] = comp;
            comp_len[ncomps]   = len;
            ++ncomps;
        }
    }

    if (ncomps == 0) {
        // "/" -- parent is root, name is "/"
        *out_name    = path;
        *out_namelen = 1;
        return ROOT_INO;
    }

    // Pass 2: walk all components except the last to find the parent directory.
    uint32_t cur_ino = ROOT_INO;
    for (int i = 0; i < ncomps - 1; ++i) {
        const char* comp = comp_start[i];
        uint8_t     len  = comp_len[i];

        if (len == 1 && comp[0] == '.') continue;
        if (len == 2 && comp[0] == '.' && comp[1] == '.') {
            RawInode* inode = inode_get(cur_ino);
            if (!inode) return 0;
            cur_ino = (inode->parent_ino != 0) ? inode->parent_ino : ROOT_INO;
            continue;
        }
        if (len > 26) return 0;
        uint32_t child = dirent_lookup(cur_ino, comp, len);
        if (child == 0) return 0;
        cur_ino = child;
    }

    // The final component is the name to create/remove.
    uint8_t final_len = comp_len[ncomps - 1];
    if (final_len > 26) return 0;

    *out_name    = comp_start[ncomps - 1];
    *out_namelen = final_len;
    return cur_ino;
}

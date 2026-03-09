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

namespace {

inline constexpr uint32_t ROOT_INO = 1;
inline constexpr uint32_t PATH_CACHE_SIZE = VFS_PROFILE_TINY ? 64U : 128U;
inline constexpr uint8_t PATH_CACHE_VALID = 0x01U;
inline constexpr uint8_t PATH_CACHE_NEGATIVE = 0x02U;

struct PathCacheEntry {
    uint32_t parent_ino;
    uint32_t child_ino;
    uint16_t hash;
    uint8_t namelen;
    uint8_t flags;
    char name[26];
};

static PathCacheEntry g_path_cache[PATH_CACHE_SIZE];

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

static uint16_t hash_component(const char* name, uint8_t namelen) {
    uint32_t hash = 2166136261u;
    for (uint8_t index = 0; index < namelen; ++index) {
        hash ^= static_cast<uint8_t>(name[index]);
        hash *= 16777619u;
    }
    return static_cast<uint16_t>((hash >> 16) ^ (hash & 0xFFFFu));
}

static uint32_t cache_slot_for(uint32_t parent_ino, uint16_t hash) {
    return (parent_ino ^ static_cast<uint32_t>(hash)) % PATH_CACHE_SIZE;
}

static bool cache_name_matches(const PathCacheEntry& entry,
                               const char* name,
                               uint8_t namelen,
                               uint16_t hash,
                               uint32_t parent_ino) {
    return (entry.flags & PATH_CACHE_VALID) != 0U &&
           entry.parent_ino == parent_ino &&
           entry.hash == hash &&
           entry.namelen == namelen &&
           __builtin_memcmp(entry.name, name, namelen) == 0;
}

static bool path_cache_lookup(uint32_t parent_ino,
                              const char* name,
                              uint8_t namelen,
                              uint32_t* child_ino,
                              bool* negative_hit) {
    if (name == nullptr || child_ino == nullptr || negative_hit == nullptr || namelen == 0U ||
        namelen > 26U) {
        return false;
    }

    const uint16_t hash = hash_component(name, namelen);
    const PathCacheEntry& entry = g_path_cache[cache_slot_for(parent_ino, hash)];
    if (!cache_name_matches(entry, name, namelen, hash, parent_ino)) {
        return false;
    }

    *child_ino = entry.child_ino;
    *negative_hit = (entry.flags & PATH_CACHE_NEGATIVE) != 0U;
    return true;
}

static void path_cache_store(uint32_t parent_ino,
                             const char* name,
                             uint8_t namelen,
                             uint32_t child_ino,
                             bool negative) {
    if (name == nullptr || namelen == 0U || namelen > 26U) {
        return;
    }

    const uint16_t hash = hash_component(name, namelen);
    PathCacheEntry& entry = g_path_cache[cache_slot_for(parent_ino, hash)];
    entry.parent_ino = parent_ino;
    entry.child_ino = child_ino;
    entry.hash = hash;
    entry.namelen = namelen;
    entry.flags = static_cast<uint8_t>(PATH_CACHE_VALID | (negative ? PATH_CACHE_NEGATIVE : 0U));
    __builtin_memset(entry.name, 0, sizeof(entry.name));
    __builtin_memcpy(entry.name, name, namelen);
}

static uint32_t resolve_child(uint32_t parent_ino, const char* name, uint8_t namelen) {
    if (namelen > 26U) {
        return 0;
    }

    uint32_t child_ino = 0;
    bool negative_hit = false;
    if (path_cache_lookup(parent_ino, name, namelen, &child_ino, &negative_hit)) {
        return negative_hit ? 0U : child_ino;
    }

    child_ino = dirent_lookup(parent_ino, name, namelen);
    path_cache_store(parent_ino, name, namelen, child_ino, child_ino == 0U);
    return child_ino;
}

} // namespace

void path_cache_reset() {
    __builtin_memset(g_path_cache, 0, sizeof(g_path_cache));
}

void path_cache_invalidate(uint32_t parent_ino, const char* name, uint8_t namelen) {
    if (name == nullptr || namelen == 0U || namelen > 26U) {
        return;
    }

    const uint16_t hash = hash_component(name, namelen);
    PathCacheEntry& entry = g_path_cache[cache_slot_for(parent_ino, hash)];
    if (cache_name_matches(entry, name, namelen, hash, parent_ino)) {
        __builtin_memset(&entry, 0, sizeof(entry));
    }
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

        uint32_t child = resolve_child(cur_ino, comp, len);
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
        uint32_t child = resolve_child(cur_ino, comp, len);
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

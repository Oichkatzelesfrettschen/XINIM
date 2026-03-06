/**
 * @file dirent.cpp
 * @brief Directory entry arena for bare-metal ramfs (ADR-0009)
 *
 * Directory entries are stored in a flat pre-allocated arena.
 * Each directory inode uses dirent_start (stored in RawInode.dirent_start)
 * as the first arena index for its entries; subsequent entries for the same
 * parent are found by scanning for matching parent_ino.
 *
 * A directory block is allocated via dirent_block_alloc() which carves out
 * a contiguous run of DIRENT_BLOCK_SIZE slots for one directory. This avoids
 * fragmentation and keeps lookups O(DIRENT_BLOCK_SIZE).
 *
 * DIRENT_BLOCK_SIZE = 32 entries per directory (1 KB per dir block).
 * A directory can be extended by chaining not needed for v1.3.0 scope.
 */

#include "bare_vfs.hpp"
#include "inode_table.hpp"
#include <cstring>

inline constexpr uint32_t DIRENT_BLOCK_SIZE = 32; // entries per directory block

// ============================================================================
// Global arena
// ============================================================================

static DirEntry g_dirent_arena[MAX_DIRENTS];
static uint32_t g_dirent_used; // next unallocated arena index

// ============================================================================
// Helpers
// ============================================================================

// Compare name[namelen] with entry->name[entry->namelen].
static inline bool name_eq(const char* name, uint8_t namelen,
                            const DirEntry* e) {
    if (e->namelen != namelen) return false;
    return (__builtin_memcmp(name, e->name, namelen) == 0);
}

// ============================================================================
// Public API
// ============================================================================

void dirent_table_init() {
    __builtin_memset(g_dirent_arena, 0, sizeof(g_dirent_arena));
    g_dirent_used = 0;
}

// Allocate DIRENT_BLOCK_SIZE slots for a new directory.
// Returns first arena index on success, MAX_DIRENTS on exhaustion.
uint32_t dirent_block_alloc() {
    if (g_dirent_used + DIRENT_BLOCK_SIZE > MAX_DIRENTS) return MAX_DIRENTS;
    uint32_t start = g_dirent_used;
    g_dirent_used += DIRENT_BLOCK_SIZE;
    return start;
}

// Add a directory entry under parent_ino with given name and child_ino.
// The dirent_start for parent must have been set via dirent_block_alloc().
// Returns 0 on success, -1 if block full or name too long.
int dirent_add(uint32_t parent_ino, uint32_t child_ino,
               uint8_t type, const char* name, uint8_t namelen) {
    if (namelen == 0 || namelen > 26) return -1;

    RawInode* parent = inode_get(parent_ino);
    if (!parent) return -1;

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) return -1;

    // Scan the block for a free slot (child_ino == 0 means unused)
    uint32_t end = start + DIRENT_BLOCK_SIZE;
    if (end > MAX_DIRENTS) end = MAX_DIRENTS;
    for (uint32_t i = start; i < end; ++i) {
        if (g_dirent_arena[i].child_ino == 0) {
            g_dirent_arena[i].child_ino  = child_ino;
            g_dirent_arena[i].type       = type;
            g_dirent_arena[i].namelen    = namelen;
            __builtin_memset(g_dirent_arena[i].name, 0, 26);
            __builtin_memcpy(g_dirent_arena[i].name, name, namelen);
            return 0;
        }
    }
    return -1; // block full
}

// Look up child inode by name in parent directory.
// Returns child ino on success, 0 if not found.
uint32_t dirent_lookup(uint32_t parent_ino, const char* name, uint8_t namelen) {
    RawInode* parent = inode_get(parent_ino);
    if (!parent) return 0;

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) return 0;

    uint32_t end = start + DIRENT_BLOCK_SIZE;
    if (end > MAX_DIRENTS) end = MAX_DIRENTS;
    for (uint32_t i = start; i < end; ++i) {
        const DirEntry* e = &g_dirent_arena[i];
        if (e->child_ino == 0) continue;
        if (name_eq(name, namelen, e)) return e->child_ino;
    }
    return 0; // not found
}

// Remove a directory entry by name from parent. Returns 0 on success, -1 if not found.
int dirent_remove(uint32_t parent_ino, const char* name, uint8_t namelen) {
    RawInode* parent = inode_get(parent_ino);
    if (!parent) return -1;

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) return -1;

    uint32_t end = start + DIRENT_BLOCK_SIZE;
    if (end > MAX_DIRENTS) end = MAX_DIRENTS;
    for (uint32_t i = start; i < end; ++i) {
        DirEntry* e = &g_dirent_arena[i];
        if (e->child_ino == 0) continue;
        if (name_eq(name, namelen, e)) {
            __builtin_memset(e, 0, sizeof(DirEntry));
            return 0;
        }
    }
    return -1; // not found
}

// Copy up to max_entries non-free entries for parent_ino into buf.
// Returns number of entries copied, or -1 on error.
int dirent_readdir(uint32_t parent_ino, DirEntry* buf, int max_entries) {
    RawInode* parent = inode_get(parent_ino);
    if (!parent || !buf || max_entries <= 0) return -1;

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) return 0;

    uint32_t end = start + DIRENT_BLOCK_SIZE;
    if (end > MAX_DIRENTS) end = MAX_DIRENTS;

    int count = 0;
    for (uint32_t i = start; i < end && count < max_entries; ++i) {
        const DirEntry* e = &g_dirent_arena[i];
        if (e->child_ino == 0) continue;
        buf[count++] = *e;
    }
    return count;
}

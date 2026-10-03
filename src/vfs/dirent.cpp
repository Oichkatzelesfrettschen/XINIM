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
 * a contiguous run of DIRENT_BLOCK_SIZE slots for one directory. Additional
 * blocks are chained on demand, so small directories stay compact while larger
 * ones do not hit a hard 32-entry ceiling.
 *
 * DIRENT_BLOCK_SIZE = 32 entries per directory (1 KB per dir block).
 * A directory can be extended by chaining not needed for v1.3.0 scope.
 */

#include "bare_vfs.hpp"
#include "inode_table.hpp"
#include "path_walk.hpp"
#include <cstring>

inline constexpr uint32_t DIRENT_BLOCK_SIZE = 32; // entries per directory block
inline constexpr uint32_t DIRENT_BLOCK_COUNT = MAX_DIRENTS / DIRENT_BLOCK_SIZE;
inline constexpr uint32_t DIRENT_BLOCK_NONE = MAX_DIRENTS;

// ============================================================================
// Global arena
// ============================================================================

static DirEntry g_dirent_arena[MAX_DIRENTS];
static uint32_t g_dirent_used; // next unallocated arena index
static uint32_t g_dirent_next_block[DIRENT_BLOCK_COUNT];

// ============================================================================
// Helpers
// ============================================================================

// Compare name[namelen] with entry->name[entry->namelen].
static inline bool name_eq(const char* name, uint8_t namelen,
                            const DirEntry* e) {
    if (e->namelen != namelen) {
        return false;
    }
    return (__builtin_memcmp(name, e->name, namelen) == 0);
}

static inline uint32_t block_index_for(uint32_t start) {
    return start / DIRENT_BLOCK_SIZE;
}

static inline uint32_t block_next(uint32_t start) {
    if (start >= MAX_DIRENTS) {
        return DIRENT_BLOCK_NONE;
    }
    return g_dirent_next_block[block_index_for(start)];
}

static inline void block_link(uint32_t start, uint32_t next_start) {
    if (start >= MAX_DIRENTS) {
        return;
    }
    g_dirent_next_block[block_index_for(start)] = next_start;
}

// ============================================================================
// Public API
// ============================================================================

void dirent_table_init() {
    __builtin_memset(g_dirent_arena, 0, sizeof(g_dirent_arena));
    for (uint32_t index = 0; index < DIRENT_BLOCK_COUNT; ++index) {
        g_dirent_next_block[index] = DIRENT_BLOCK_NONE;
    }
    g_dirent_used = 0;
    path_cache_reset();
}

// Allocate DIRENT_BLOCK_SIZE slots for a new directory.
// Returns first arena index on success, MAX_DIRENTS on exhaustion.
uint32_t dirent_block_alloc() {
    if (g_dirent_used + DIRENT_BLOCK_SIZE > MAX_DIRENTS) {
        return MAX_DIRENTS;
    }
    uint32_t start = g_dirent_used;
    g_dirent_used += DIRENT_BLOCK_SIZE;
    g_dirent_next_block[block_index_for(start)] = DIRENT_BLOCK_NONE;
    return start;
}

// Add a directory entry under parent_ino with given name and child_ino.
// The dirent_start for parent must have been set via dirent_block_alloc().
// Returns 0 on success, -1 if block full or name too long.
int dirent_add(uint32_t parent_ino, uint32_t child_ino,
               uint8_t type, const char* name, uint8_t namelen) {
    if (namelen == 0 || namelen > 26) {
        return -1;
    }

    RawInode* parent = inode_get(parent_ino);
    if (!parent) {
        return -1;
    }

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) {
        return -1;
    }

    uint32_t block_start = start;
    for (;;) {
        uint32_t end = block_start + DIRENT_BLOCK_SIZE;
        if (end > MAX_DIRENTS) {
            end = MAX_DIRENTS;
        }
        for (uint32_t i = block_start; i < end; ++i) {
            if (g_dirent_arena[i].child_ino == 0) {
                g_dirent_arena[i].child_ino  = child_ino;
                g_dirent_arena[i].type       = type;
                g_dirent_arena[i].namelen    = namelen;
                __builtin_memset(g_dirent_arena[i].name, 0, 26);
                __builtin_memcpy(g_dirent_arena[i].name, name, namelen);
                path_cache_invalidate(parent_ino, name, namelen);
                return 0;
            }
        }

        const uint32_t next_block = block_next(block_start);
        if (next_block != DIRENT_BLOCK_NONE) {
            block_start = next_block;
            continue;
        }

        const uint32_t new_block = dirent_block_alloc();
        if (new_block >= MAX_DIRENTS) {
            return -1;
        }
        block_link(block_start, new_block);
        block_start = new_block;
    }
}

// Look up child inode by name in parent directory.
// Returns child ino on success, 0 if not found.
uint32_t dirent_lookup(uint32_t parent_ino, const char* name, uint8_t namelen) {
    RawInode* parent = inode_get(parent_ino);
    if (!parent) {
        return 0;
    }

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) {
        return 0;
    }

    uint32_t block_start = start;
    while (block_start < MAX_DIRENTS) {
        uint32_t end = block_start + DIRENT_BLOCK_SIZE;
        if (end > MAX_DIRENTS) {
            end = MAX_DIRENTS;
        }
        for (uint32_t i = block_start; i < end; ++i) {
            const DirEntry* e = &g_dirent_arena[i];
            if (e->child_ino == 0) {
                continue;
            }
            if (name_eq(name, namelen, e)) {
                return e->child_ino;
            }
        }
        const uint32_t next_block = block_next(block_start);
        if (next_block == DIRENT_BLOCK_NONE) {
            break;
        }
        block_start = next_block;
    }
    return 0; // not found
}

// Remove a directory entry by name from parent. Returns 0 on success, -1 if not found.
int dirent_remove(uint32_t parent_ino, const char* name, uint8_t namelen) {
    RawInode* parent = inode_get(parent_ino);
    if (!parent) {
        return -1;
    }

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) {
        return -1;
    }

    uint32_t block_start = start;
    while (block_start < MAX_DIRENTS) {
        uint32_t end = block_start + DIRENT_BLOCK_SIZE;
        if (end > MAX_DIRENTS) {
            end = MAX_DIRENTS;
        }
        for (uint32_t i = block_start; i < end; ++i) {
            DirEntry* e = &g_dirent_arena[i];
            if (e->child_ino == 0) {
                continue;
            }
            if (name_eq(name, namelen, e)) {
                __builtin_memset(e, 0, sizeof(DirEntry));
                path_cache_invalidate(parent_ino, name, namelen);
                return 0;
            }
        }
        const uint32_t next_block = block_next(block_start);
        if (next_block == DIRENT_BLOCK_NONE) {
            break;
        }
        block_start = next_block;
    }
    return -1; // not found
}

// Copy up to max_entries non-free entries for parent_ino into buf.
// Returns number of entries copied, or -1 on error.
int dirent_readdir(uint32_t parent_ino, DirEntry* buf, int max_entries) {
    RawInode* parent = inode_get(parent_ino);
    if (!parent || !buf || max_entries <= 0) {
        return -1;
    }

    uint32_t start = static_cast<uint32_t>(parent->dirent_start);
    if (start >= MAX_DIRENTS) {
        return 0;
    }

    int count = 0;
    uint32_t block_start = start;
    while (block_start < MAX_DIRENTS && count < max_entries) {
        uint32_t end = block_start + DIRENT_BLOCK_SIZE;
        if (end > MAX_DIRENTS) {
            end = MAX_DIRENTS;
        }

        for (uint32_t i = block_start; i < end && count < max_entries; ++i) {
            const DirEntry* e = &g_dirent_arena[i];
            if (e->child_ino == 0) {
                continue;
            }
            buf[count++] = *e;
        }

        const uint32_t next_block = block_next(block_start);
        if (next_block == DIRENT_BLOCK_NONE) {
            break;
        }
        block_start = next_block;
    }
    return count;
}

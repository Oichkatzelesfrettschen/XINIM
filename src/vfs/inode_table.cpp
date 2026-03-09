/**
 * @file inode_table.cpp
 * @brief Flat inode allocator and data arena for bare-metal ramfs (ADR-0009)
 *
 * All storage is statically allocated. Zero heap usage. Zero STL.
 * Inode 0 is permanently reserved (invalid sentinel).
 * Inode 1 is the root directory, initialised by vfs_server_init().
 */

#include "bare_vfs.hpp"
#include "vnode_table.hpp"
#include <cstring>

// sizeof(RawInode) is 80 bytes with the current field layout
// (uint64_t fields pull alignment). alignas(64) still gives us
// cache-line-aligned starts for each inode in the array.
static_assert(sizeof(RawInode) >= 64, "RawInode must be at least 64 bytes");
static_assert(alignof(RawInode) == 64, "RawInode must be 64-byte aligned");

// ============================================================================
// Global storage
// ============================================================================

static RawInode g_inodes[MAX_INODES];

// Bitmap: bit i of word (i/64) corresponds to inode i. 1 = used.
static uint64_t g_inode_bitmap[MAX_INODES / 64]; // 16 words = 128 bytes

// 1 MB data arena for file bodies too large for inline_data (>24 bytes).
// Block unit: CACHE_BLK_SIZE (512 bytes). data_block_off in RawInode is
// a byte offset into this arena.
alignas(CACHE_BLK_SIZE) uint8_t g_data_arena[DATA_ARENA_SIZE];
static uint32_t g_data_arena_used; // next free byte (bump pointer)

// Free list for reclaimed arena regions.
// Stored as (offset, size) pairs; up to DATA_FREE_SLOTS entries.
// Insertions try to coalesce with adjacent entries (O(n) scan).
static constexpr uint32_t DATA_FREE_SLOTS = 256;
struct ArenaFreeSlot { uint32_t off; uint32_t sz; };
static ArenaFreeSlot g_arena_free[DATA_FREE_SLOTS];
static uint32_t      g_arena_free_count = 0;

// ============================================================================
// Bitmap helpers
// ============================================================================

static inline void bitmap_set(uint32_t idx) {
    g_inode_bitmap[idx >> 6] |= (1ULL << (idx & 63u));
}

static inline void bitmap_clear(uint32_t idx) {
    g_inode_bitmap[idx >> 6] &= ~(1ULL << (idx & 63u));
}

static inline bool bitmap_test(uint32_t idx) {
    return (g_inode_bitmap[idx >> 6] & (1ULL << (idx & 63u))) != 0;
}

// ============================================================================
// Public API
// ============================================================================

void inode_table_init() {
    __builtin_memset(g_inodes, 0, sizeof(g_inodes));
    __builtin_memset(g_inode_bitmap, 0, sizeof(g_inode_bitmap));
    __builtin_memset(g_data_arena, 0, sizeof(g_data_arena));
    __builtin_memset(g_arena_free, 0, sizeof(g_arena_free));
    g_data_arena_used  = 0;
    g_arena_free_count = 0;

    // Reserve slot 0: inode 0 is the invalid/free sentinel
    bitmap_set(0);
    g_inodes[0].ino    = 0;
    g_inodes[0].iflags = INODE_IS_USED;
}

// Allocate a free inode slot. Returns ino (>= 1) on success, 0 on exhaustion.
uint32_t inode_alloc() {
    for (uint32_t word = 0; word < (MAX_INODES / 64); ++word) {
        uint64_t free_bits = ~g_inode_bitmap[word];
        if (free_bits == 0) continue;
        uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(free_bits));
        uint32_t ino = (word << 6) | bit;
        if (ino == 0 || ino >= MAX_INODES) continue; // sanity
        bitmap_set(ino);
        __builtin_memset(&g_inodes[ino], 0, sizeof(RawInode));
        g_inodes[ino].ino    = ino;
        g_inodes[ino].nlink  = 1;
        g_inodes[ino].iflags = INODE_IS_USED;
        return ino;
    }
    return 0; // exhausted
}

// Free inode slot. Validates ino, clears bitmap bit and zeroes entry.
void inode_free(uint32_t ino) {
    if (ino == 0 || ino >= MAX_INODES) return;
    if (!bitmap_test(ino)) return; // double-free guard
    vnode_forget(ino);
    bitmap_clear(ino);
    __builtin_memset(&g_inodes[ino], 0, sizeof(RawInode));
}

// Return pointer to inode entry. Returns nullptr for invalid ino.
RawInode* inode_get(uint32_t ino) {
    if (ino == 0 || ino >= MAX_INODES) return nullptr;
    if (!bitmap_test(ino)) return nullptr;
    return &g_inodes[ino];
}

// Allocate len bytes from the data arena.
// First-fit search through the free list; falls back to bump allocator.
// Returns byte offset into g_data_arena on success, DATA_ARENA_SIZE on exhaustion.
// All allocations are CACHE_BLK_SIZE-aligned.
uint32_t data_arena_alloc(uint32_t len) {
    if (len == 0) return DATA_ARENA_SIZE;
    uint32_t aligned = (len + CACHE_BLK_SIZE - 1u) & ~(CACHE_BLK_SIZE - 1u);

    // First-fit search in free list
    for (uint32_t i = 0; i < g_arena_free_count; ++i) {
        if (g_arena_free[i].sz >= aligned) {
            uint32_t off = g_arena_free[i].off;
            uint32_t remainder = g_arena_free[i].sz - aligned;
            if (remainder > 0) {
                // Split: shrink this slot
                g_arena_free[i].off += aligned;
                g_arena_free[i].sz   = remainder;
            } else {
                // Exact fit: remove slot (swap with last)
                g_arena_free[i] = g_arena_free[--g_arena_free_count];
            }
            return off;
        }
    }

    // Bump allocator fallback
    if (g_data_arena_used + aligned > DATA_ARENA_SIZE) return DATA_ARENA_SIZE;
    uint32_t off = g_data_arena_used;
    g_data_arena_used += aligned;
    return off;
}

// Return a region to the arena free list.
// Merges with adjacent free entries (forward and backward) to limit fragmentation.
// Silently leaks if the free list is full (never crashes).
void data_arena_free(uint32_t off, uint32_t len) {
    if (off >= DATA_ARENA_SIZE || len == 0) return;
    uint32_t aligned = (len + CACHE_BLK_SIZE - 1u) & ~(CACHE_BLK_SIZE - 1u);
    if (off + aligned > DATA_ARENA_SIZE) return;

    // Attempt to coalesce with an existing free slot that is adjacent.
    for (uint32_t i = 0; i < g_arena_free_count; ++i) {
        // Merge: existing slot immediately precedes the freed region
        if (g_arena_free[i].off + g_arena_free[i].sz == off) {
            g_arena_free[i].sz += aligned;
            // Check if this newly extended slot now also touches the next slot
            for (uint32_t j = 0; j < g_arena_free_count; ++j) {
                if (j == i) continue;
                if (g_arena_free[i].off + g_arena_free[i].sz == g_arena_free[j].off) {
                    g_arena_free[i].sz += g_arena_free[j].sz;
                    g_arena_free[j] = g_arena_free[--g_arena_free_count];
                    break;
                }
            }
            return;
        }
        // Merge: freed region immediately precedes the existing slot
        if (off + aligned == g_arena_free[i].off) {
            g_arena_free[i].off  = off;
            g_arena_free[i].sz  += aligned;
            return;
        }
    }

    // No adjacent slot found: add a new free entry if space permits
    if (g_arena_free_count < DATA_FREE_SLOTS) {
        g_arena_free[g_arena_free_count++] = { off, aligned };
    }
    // If free list is full we leak this region (acceptable: avoids crash)
}

// Return pointer to data arena at given byte offset.
uint8_t* data_arena_ptr(uint32_t off) {
    if (off >= DATA_ARENA_SIZE) return nullptr;
    return g_data_arena + off;
}

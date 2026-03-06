/**
 * @file inode_table.cpp
 * @brief Flat inode allocator and data arena for bare-metal ramfs (ADR-0009)
 *
 * All storage is statically allocated. Zero heap usage. Zero STL.
 * Inode 0 is permanently reserved (invalid sentinel).
 * Inode 1 is the root directory, initialised by vfs_server_init().
 */

#include "bare_vfs.hpp"
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
static uint32_t g_data_arena_used; // next free byte offset

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
    g_data_arena_used = 0;

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
    bitmap_clear(ino);
    __builtin_memset(&g_inodes[ino], 0, sizeof(RawInode));
}

// Return pointer to inode entry. Returns nullptr for invalid ino.
RawInode* inode_get(uint32_t ino) {
    if (ino == 0 || ino >= MAX_INODES) return nullptr;
    if (!bitmap_test(ino)) return nullptr;
    return &g_inodes[ino];
}

// Allocate len bytes from the data arena. Returns byte offset into
// g_data_arena on success, DATA_ARENA_SIZE on exhaustion.
// Allocations are CACHE_BLK_SIZE-aligned.
uint32_t data_arena_alloc(uint32_t len) {
    // Round len up to next CACHE_BLK_SIZE boundary
    uint32_t aligned = (len + CACHE_BLK_SIZE - 1u) & ~(CACHE_BLK_SIZE - 1u);
    if (g_data_arena_used + aligned > DATA_ARENA_SIZE) return DATA_ARENA_SIZE; // full
    uint32_t off = g_data_arena_used;
    g_data_arena_used += aligned;
    return off;
}

// Return pointer to data arena at given byte offset.
uint8_t* data_arena_ptr(uint32_t off) {
    if (off >= DATA_ARENA_SIZE) return nullptr;
    return g_data_arena + off;
}

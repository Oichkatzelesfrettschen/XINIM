/**
 * @file buffer_cache.cpp
 * @brief Pre-allocated buffer cache for bare-metal VFS (ADR-0009)
 *
 * Replaces the previous STL-based BufferCache class (which used std::mutex,
 * std::list, std::unordered_map -- all incompatible with -ffreestanding).
 *
 * Design: 64 fixed-size 512-byte cache slots. LRU eviction by lowest
 * lru_seq counter: O(CACHE_BLOCKS) scan on miss. Zero malloc during I/O.
 *
 * For ramfs (device_id=1) the backing store is g_data_arena in inode_table.cpp.
 * block_num is the block index (byte_offset / CACHE_BLK_SIZE) into the arena.
 */

#include "bare_vfs.hpp"
#include "inode_table.hpp"
#include <cstring>

inline constexpr uint64_t CACHE_BLOCK_FREE = ~uint64_t{0}; // UINT64_MAX

// ============================================================================
// Global cache storage
// ============================================================================

static CacheBlock g_cache[CACHE_BLOCKS];
static uint32_t   g_lru_seq; // global monotonically-increasing LRU counter

// ============================================================================
// Public API
// ============================================================================

void cache_init() {
    if constexpr (!VFS_BUFFER_CACHE_ENABLED) {
        return;
    }
    __builtin_memset(g_cache, 0, sizeof(g_cache));
    for (uint32_t i = 0; i < CACHE_BLOCKS; ++i) {
        g_cache[i].block_num = CACHE_BLOCK_FREE;
    }
    g_lru_seq = 0;
}

// Return (and potentially load) a cache block for (block_num, device_id).
// On hit: update lru_seq and return pointer.
// On miss: evict LRU block, load data from data_arena, return pointer.
// Returns nullptr if block_num is out of range for device.
CacheBlock* cache_get(uint64_t block_num, uint32_t device_id) {
    if constexpr (!VFS_BUFFER_CACHE_ENABLED) {
        (void)block_num;
        (void)device_id;
        return nullptr;
    }

    // Only device_id=1 (ramfs data arena) supported in v1.3.0
    if (device_id != 1) {
        return nullptr;
    }

    uint64_t max_block = DATA_ARENA_SIZE / CACHE_BLK_SIZE;
    if (block_num >= max_block) {
        return nullptr;
    }

    uint32_t tick = ++g_lru_seq;

    // Cache hit scan
    for (uint32_t i = 0; i < CACHE_BLOCKS; ++i) {
        if (g_cache[i].block_num == block_num &&
            g_cache[i].device_id == device_id) {
            g_cache[i].lru_seq = tick;
            return &g_cache[i];
        }
    }

    // Cache miss -- find LRU slot to evict.
    // Free slots (block_num == CACHE_BLOCK_FREE) are preferred.
    uint32_t evict   = 0;
    uint32_t min_seq = g_cache[0].lru_seq;
    for (uint32_t i = 0; i < CACHE_BLOCKS; ++i) {
        if (g_cache[i].block_num == CACHE_BLOCK_FREE) {
            evict = i;
            min_seq = 0; // free slot terminates search
            break;
        }
        if (g_cache[i].lru_seq < min_seq) {
            min_seq = g_cache[i].lru_seq;
            evict   = i;
        }
    }

    // Flush dirty evicted block back to arena before replacing
    CacheBlock* slot = &g_cache[evict];
    if (slot->block_num != CACHE_BLOCK_FREE && slot->dirty) {
        uint8_t* dst = data_arena_ptr(
            static_cast<uint32_t>(slot->block_num * CACHE_BLK_SIZE));
        if (dst) {
            __builtin_memcpy(dst, slot->data, CACHE_BLK_SIZE);
        }
        slot->dirty = 0;
    }

    // Load requested block from arena into the slot
    uint8_t* src = data_arena_ptr(static_cast<uint32_t>(block_num * CACHE_BLK_SIZE));
    if (src) {
        __builtin_memcpy(slot->data, src, CACHE_BLK_SIZE);
    } else {
        __builtin_memset(slot->data, 0, CACHE_BLK_SIZE);
    }

    slot->block_num  = block_num;
    slot->device_id  = device_id;
    slot->lru_seq    = tick;
    slot->dirty      = 0;
    return slot;
}

// Mark block as dirty (data will be written back on eviction or flush).
void cache_mark_dirty(CacheBlock* block) {
    if constexpr (!VFS_BUFFER_CACHE_ENABLED) {
        (void)block;
        return;
    }
    if (block) {
        block->dirty = 1;
    }
}

// Write all dirty blocks for device_id back to the backing store.
void cache_flush(uint32_t device_id) {
    if constexpr (!VFS_BUFFER_CACHE_ENABLED) {
        (void)device_id;
        return;
    }
    for (uint32_t i = 0; i < CACHE_BLOCKS; ++i) {
        CacheBlock* slot = &g_cache[i];
        if (slot->block_num == CACHE_BLOCK_FREE) {
            continue;
        }
        if (slot->device_id != device_id) {
            continue;
        }
        if (!slot->dirty) {
            continue;
        }

        uint8_t* dst = data_arena_ptr(
            static_cast<uint32_t>(slot->block_num * CACHE_BLK_SIZE));
        if (dst) {
            __builtin_memcpy(dst, slot->data, CACHE_BLK_SIZE);
        }
        slot->dirty = 0;
    }
}

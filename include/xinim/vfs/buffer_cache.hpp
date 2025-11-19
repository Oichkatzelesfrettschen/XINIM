/**
 * @file buffer_cache.hpp
 * @brief Block Buffer Cache for VFS Layer
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * High-performance block buffer cache with LRU eviction and write-back support.
 * Provides O(1) lookups and significantly reduces disk I/O operations.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>

namespace xinim::vfs {

/**
 * @brief Cached block buffer
 *
 * Represents a single cached block with metadata for LRU management
 * and dirty tracking.
 */
struct CachedBlock {
    uint64_t device_id;      ///< Device identifier
    uint64_t block_number;   ///< Block number on device
    uint32_t block_size;     ///< Size of block in bytes
    std::vector<uint8_t> data; ///< Block data
    bool dirty;              ///< Dirty flag (needs write-back)
    uint32_t ref_count;      ///< Reference count for pinning

    /**
     * @brief Construct a cached block
     * @param dev Device ID
     * @param blk Block number
     * @param size Block size in bytes
     */
    CachedBlock(uint64_t dev, uint64_t blk, uint32_t size)
        : device_id(dev), block_number(blk), block_size(size),
          data(size), dirty(false), ref_count(0) {}
};

/**
 * @brief Cache key for hash map lookups
 */
struct BlockKey {
    uint64_t device_id;
    uint64_t block_number;

    bool operator==(const BlockKey& other) const {
        return device_id == other.device_id && block_number == other.block_number;
    }
};

/**
 * @brief Hash function for BlockKey
 */
struct BlockKeyHash {
    std::size_t operator()(const BlockKey& key) const {
        // Combine device_id and block_number for hash
        return std::hash<uint64_t>()(key.device_id) ^
               (std::hash<uint64_t>()(key.block_number) << 1);
    }
};

/**
 * @brief Block buffer cache with LRU eviction
 *
 * Thread-safe block cache that provides:
 * - O(1) lookups via hash map
 * - LRU eviction policy
 * - Write-back caching for performance
 * - Dirty block tracking and synchronization
 * - Reference counting for block pinning
 */
class BufferCache {
public:
    /**
     * @brief Construct buffer cache
     * @param max_blocks Maximum number of blocks to cache
     */
    explicit BufferCache(size_t max_blocks = 1024);

    /**
     * @brief Destroy buffer cache (flushes all dirty blocks)
     */
    ~BufferCache();

    /**
     * @brief Get a block from cache or read from device
     * @param device_id Device identifier
     * @param block_number Block number to read
     * @param block_size Size of block in bytes
     * @param data Output buffer for block data
     * @return 0 on success, negative error code on failure
     *
     * If block is in cache, returns cached data immediately.
     * Otherwise, invokes read_callback to fetch from device.
     */
    int get_block(uint64_t device_id, uint64_t block_number,
                  uint32_t block_size, void* data);

    /**
     * @brief Write a block to cache
     * @param device_id Device identifier
     * @param block_number Block number to write
     * @param block_size Size of block in bytes
     * @param data Block data to write
     * @param write_through If true, write immediately to device
     * @return 0 on success, negative error code on failure
     *
     * Writes to cache and marks block as dirty (unless write_through).
     * Dirty blocks are flushed during sync() or eviction.
     */
    int put_block(uint64_t device_id, uint64_t block_number,
                  uint32_t block_size, const void* data,
                  bool write_through = false);

    /**
     * @brief Synchronize all dirty blocks to device
     * @return 0 on success, negative error code on failure
     *
     * Flushes all dirty blocks to their respective devices.
     */
    int sync();

    /**
     * @brief Synchronize specific device's dirty blocks
     * @param device_id Device to synchronize
     * @return 0 on success, negative error code on failure
     */
    int sync_device(uint64_t device_id);

    /**
     * @brief Invalidate (remove) a block from cache
     * @param device_id Device identifier
     * @param block_number Block number to invalidate
     *
     * Removes block from cache. If block is dirty, flushes first.
     */
    void invalidate_block(uint64_t device_id, uint64_t block_number);

    /**
     * @brief Invalidate all blocks for a device
     * @param device_id Device identifier
     *
     * Removes all cached blocks for device. Flushes dirty blocks first.
     */
    void invalidate_device(uint64_t device_id);

    /**
     * @brief Get cache statistics
     * @param hits Output: number of cache hits
     * @param misses Output: number of cache misses
     * @param evictions Output: number of evictions
     * @param dirty_blocks Output: current number of dirty blocks
     */
    void get_stats(uint64_t& hits, uint64_t& misses, uint64_t& evictions,
                   size_t& dirty_blocks) const;

    /**
     * @brief Set read callback for device I/O
     * @param callback Function to read block from device
     *
     * Callback signature: int(uint64_t dev, uint64_t blk, uint32_t size, void* buf)
     */
    void set_read_callback(int (*callback)(uint64_t, uint64_t, uint32_t, void*));

    /**
     * @brief Set write callback for device I/O
     * @param callback Function to write block to device
     *
     * Callback signature: int(uint64_t dev, uint64_t blk, uint32_t size, const void* buf)
     */
    void set_write_callback(int (*callback)(uint64_t, uint64_t, uint32_t, const void*));

private:
    // LRU list: most recently used at front
    using LRUList = std::list<std::shared_ptr<CachedBlock>>;
    using LRUIter = LRUList::iterator;

    // Hash map for O(1) lookups
    std::unordered_map<BlockKey, LRUIter, BlockKeyHash> cache_map_;

    // LRU list
    LRUList lru_list_;

    // Maximum cache size
    size_t max_blocks_;

    // Statistics
    mutable uint64_t cache_hits_;
    mutable uint64_t cache_misses_;
    uint64_t evictions_;

    // Callbacks for device I/O
    int (*read_callback_)(uint64_t, uint64_t, uint32_t, void*);
    int (*write_callback_)(uint64_t, uint64_t, uint32_t, const void*);

    // Thread safety
    mutable std::mutex mutex_;

    /**
     * @brief Evict least recently used block
     * @return true if eviction successful, false if no evictable blocks
     */
    bool evict_lru();

    /**
     * @brief Move block to front of LRU list (mark as recently used)
     * @param it Iterator to block in LRU list
     */
    void touch_block(LRUIter it);

    /**
     * @brief Flush a dirty block to device
     * @param block Block to flush
     * @return 0 on success, negative error code on failure
     */
    int flush_block(std::shared_ptr<CachedBlock> block);
};

/**
 * @brief Global buffer cache instance
 *
 * Singleton instance shared across all filesystems.
 */
BufferCache& get_global_buffer_cache();

} // namespace xinim::vfs

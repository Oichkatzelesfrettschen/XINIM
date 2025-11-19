/**
 * @file buffer_cache.cpp
 * @brief Block Buffer Cache Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/buffer_cache.hpp>
#include <xinim/log.hpp>
#include <cstring>

namespace xinim::vfs {

BufferCache::BufferCache(size_t max_blocks)
    : max_blocks_(max_blocks),
      cache_hits_(0),
      cache_misses_(0),
      evictions_(0),
      read_callback_(nullptr),
      write_callback_(nullptr) {
}

BufferCache::~BufferCache() {
    // Flush all dirty blocks before destruction
    sync();
}

int BufferCache::get_block(uint64_t device_id, uint64_t block_number,
                           uint32_t block_size, void* data) {
    std::lock_guard<std::mutex> lock(mutex_);

    BlockKey key{device_id, block_number};
    auto it = cache_map_.find(key);

    if (it != cache_map_.end()) {
        // Cache hit!
        cache_hits_++;

        auto block = *it->second;
        std::memcpy(data, block->data.data(), block_size);

        // Move to front of LRU list (mark as recently used)
        touch_block(it->second);

        return 0;
    }

    // Cache miss - need to read from device
    cache_misses_++;

    if (!read_callback_) {
        LOG_ERROR("buffer_cache: No read callback set");
        return -EIO;
    }

    // Evict if cache is full
    if (cache_map_.size() >= max_blocks_) {
        if (!evict_lru()) {
            LOG_WARN("buffer_cache: Failed to evict block, cache full");
            // Continue anyway - we'll just exceed cache size slightly
        }
    }

    // Create new cached block
    auto new_block = std::make_shared<CachedBlock>(device_id, block_number, block_size);

    // Read from device
    int ret = read_callback_(device_id, block_number, block_size, new_block->data.data());
    if (ret < 0) {
        LOG_ERROR("buffer_cache: Failed to read block %lu from device %lu: %d",
                  block_number, device_id, ret);
        return ret;
    }

    // Copy data to output
    std::memcpy(data, new_block->data.data(), block_size);

    // Add to cache
    lru_list_.push_front(new_block);
    cache_map_[key] = lru_list_.begin();

    return 0;
}

int BufferCache::put_block(uint64_t device_id, uint64_t block_number,
                           uint32_t block_size, const void* data,
                           bool write_through) {
    std::lock_guard<std::mutex> lock(mutex_);

    BlockKey key{device_id, block_number};
    auto it = cache_map_.find(key);

    std::shared_ptr<CachedBlock> block;

    if (it != cache_map_.end()) {
        // Block already in cache - update it
        block = *it->second;
        std::memcpy(block->data.data(), data, block_size);
        block->dirty = !write_through;

        // Move to front of LRU list
        touch_block(it->second);
    } else {
        // Block not in cache - create new entry

        // Evict if cache is full
        if (cache_map_.size() >= max_blocks_) {
            if (!evict_lru()) {
                LOG_WARN("buffer_cache: Failed to evict block, cache full");
            }
        }

        // Create new block
        block = std::make_shared<CachedBlock>(device_id, block_number, block_size);
        std::memcpy(block->data.data(), data, block_size);
        block->dirty = !write_through;

        // Add to cache
        lru_list_.push_front(block);
        cache_map_[key] = lru_list_.begin();
    }

    // Write through if requested
    if (write_through) {
        if (!write_callback_) {
            LOG_ERROR("buffer_cache: No write callback set");
            return -EIO;
        }

        int ret = write_callback_(device_id, block_number, block_size, data);
        if (ret < 0) {
            LOG_ERROR("buffer_cache: Failed to write block %lu to device %lu: %d",
                      block_number, device_id, ret);
            return ret;
        }
    }

    return 0;
}

int BufferCache::sync() {
    std::lock_guard<std::mutex> lock(mutex_);

    int errors = 0;

    // Flush all dirty blocks
    for (auto& block : lru_list_) {
        if (block->dirty) {
            int ret = flush_block(block);
            if (ret < 0) {
                errors++;
            }
        }
    }

    return errors > 0 ? -EIO : 0;
}

int BufferCache::sync_device(uint64_t device_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    int errors = 0;

    // Flush dirty blocks for specific device
    for (auto& block : lru_list_) {
        if (block->device_id == device_id && block->dirty) {
            int ret = flush_block(block);
            if (ret < 0) {
                errors++;
            }
        }
    }

    return errors > 0 ? -EIO : 0;
}

void BufferCache::invalidate_block(uint64_t device_id, uint64_t block_number) {
    std::lock_guard<std::mutex> lock(mutex_);

    BlockKey key{device_id, block_number};
    auto it = cache_map_.find(key);

    if (it != cache_map_.end()) {
        auto block = *it->second;

        // Flush if dirty
        if (block->dirty) {
            flush_block(block);
        }

        // Remove from LRU list and cache map
        lru_list_.erase(it->second);
        cache_map_.erase(it);
    }
}

void BufferCache::invalidate_device(uint64_t device_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find all blocks for this device
    auto it = lru_list_.begin();
    while (it != lru_list_.end()) {
        auto block = *it;

        if (block->device_id == device_id) {
            // Flush if dirty
            if (block->dirty) {
                flush_block(block);
            }

            // Remove from cache map
            BlockKey key{device_id, block->block_number};
            cache_map_.erase(key);

            // Remove from LRU list
            it = lru_list_.erase(it);
        } else {
            ++it;
        }
    }
}

void BufferCache::get_stats(uint64_t& hits, uint64_t& misses, uint64_t& evictions,
                            size_t& dirty_blocks) const {
    std::lock_guard<std::mutex> lock(mutex_);

    hits = cache_hits_;
    misses = cache_misses_;
    evictions = evictions_;

    // Count dirty blocks
    dirty_blocks = 0;
    for (const auto& block : lru_list_) {
        if (block->dirty) {
            dirty_blocks++;
        }
    }
}

void BufferCache::set_read_callback(int (*callback)(uint64_t, uint64_t, uint32_t, void*)) {
    std::lock_guard<std::mutex> lock(mutex_);
    read_callback_ = callback;
}

void BufferCache::set_write_callback(int (*callback)(uint64_t, uint64_t, uint32_t, const void*)) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_callback_ = callback;
}

bool BufferCache::evict_lru() {
    // Find least recently used block that is not pinned
    for (auto it = lru_list_.rbegin(); it != lru_list_.rend(); ++it) {
        auto block = *it;

        // Skip pinned blocks
        if (block->ref_count > 0) {
            continue;
        }

        // Flush if dirty
        if (block->dirty) {
            int ret = flush_block(block);
            if (ret < 0) {
                LOG_WARN("buffer_cache: Failed to flush block during eviction");
                continue;  // Try next block
            }
        }

        // Remove from cache map
        BlockKey key{block->device_id, block->block_number};
        cache_map_.erase(key);

        // Remove from LRU list (convert reverse iterator to forward iterator)
        lru_list_.erase(std::next(it).base());

        evictions_++;
        return true;
    }

    return false;  // No evictable blocks
}

void BufferCache::touch_block(LRUIter it) {
    // Move block to front of LRU list
    lru_list_.splice(lru_list_.begin(), lru_list_, it);
}

int BufferCache::flush_block(std::shared_ptr<CachedBlock> block) {
    if (!write_callback_) {
        LOG_ERROR("buffer_cache: No write callback set");
        return -EIO;
    }

    int ret = write_callback_(block->device_id, block->block_number,
                             block->block_size, block->data.data());
    if (ret < 0) {
        LOG_ERROR("buffer_cache: Failed to flush block %lu on device %lu: %d",
                  block->block_number, block->device_id, ret);
        return ret;
    }

    block->dirty = false;
    return 0;
}

// Global buffer cache instance
static BufferCache global_cache(1024);  // Cache 1024 blocks (4 MB for 4KB blocks)

BufferCache& get_global_buffer_cache() {
    return global_cache;
}

} // namespace xinim::vfs

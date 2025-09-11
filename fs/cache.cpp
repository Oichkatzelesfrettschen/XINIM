/**
 * @file cache.cpp
 * @brief Buffer cache implementation providing block I/O caching.
 */

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "const.hpp"
#include "dev.hpp"
#include "glo.hpp"
#include "super.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

using IoMode = minix::fs::DefaultFsConstants::IoMode;

/** @brief Forward declaration for put_block used by BufferGuard. */
void put_block(struct buf *bp, BlockType block_type);

/**
 * @class BufferGuard
 * @brief RAII wrapper that releases a buffer when leaving scope.
 *
 * @param bp   Managed buffer pointer.
 * @param type Usage hint for ::put_block upon destruction.
 */
class BufferGuard {
    struct buf *bp_;
    BlockType type_;

  public:
    /**
     * @brief Construct a guard for a buffer.
     * @param bp   Pointer to the buffer to manage.
     * @param type Usage hint passed to ::put_block on destruction.
     */
    explicit BufferGuard(struct buf *bp = NIL_BUF, BlockType type = BlockType::FullData) noexcept
        : bp_{bp}, type_{type} {}

    /** @brief Release the buffer on destruction. */
    ~BufferGuard() {
        if (bp_ != NIL_BUF) {
            put_block(bp_, type_);
        }
    }

    BufferGuard(const BufferGuard &) = delete;
    BufferGuard &operator=(const BufferGuard &) = delete;

    /**
     * @brief Move constructor.
     * @param other Guard to move from.
     */
    BufferGuard(BufferGuard &&other) noexcept
        : bp_{std::exchange(other.bp_, NIL_BUF)}, type_{other.type_} {}

    /**
     * @brief Move assignment operator.
     * @param other Guard to move from.
     * @return Reference to this guard.
     */
    BufferGuard &operator=(BufferGuard &&other) noexcept {
        if (this != &other) {
            if (bp_ != NIL_BUF) {
                put_block(bp_, type_);
            }
            bp_ = std::exchange(other.bp_, NIL_BUF);
            type_ = other.type_;
        }
        return *this;
    }

    /**
     * @brief Obtain the managed buffer.
     * @return Raw buffer pointer.
     */
    [[nodiscard]] struct buf *get() const noexcept { return bp_; }

    /**
     * @brief Release ownership without calling ::put_block.
     * @return Raw buffer pointer previously managed.
     */
    [[nodiscard]] struct buf *release() noexcept { return std::exchange(bp_, NIL_BUF); }
};

namespace {
/** @brief Hash mask assuming ::kNrBufHash is power of two. */
constexpr std::size_t kHashMask = static_cast<std::size_t>(kNrBufHash - 1);

/**
 * @brief Compute hash table index for a block number.
 * @param block Block number.
 * @return Hash table index.
 */
constexpr std::size_t hash_index(block_nr block) noexcept {
    return static_cast<std::size_t>(block) & kHashMask;
}

/**
 * @brief Check whether a ::BlockType contains a specific flag.
 * @param type Block type to test.
 * @param flag Flag to look for.
 * @return True if the flag is set.
 */
constexpr bool has_flag(BlockType type, BlockType flag) noexcept {
    using Under = std::underlying_type_t<BlockType>;
    return (static_cast<Under>(type) & static_cast<Under>(flag)) != 0;
}

/**
 * @brief Detach a buffer from the LRU list.
 * @param bp Buffer to detach.
 */
void remove_from_lru(struct buf *bp) noexcept {
    if (bp->b_prev) {
        bp->b_prev->b_next = bp->b_next;
    } else {
        front = bp->b_next;
    }
    if (bp->b_next) {
        bp->b_next->b_prev = bp->b_prev;
    } else {
        rear = bp->b_prev;
    }
    bp->b_next = bp->b_prev = NIL_BUF;
}

/**
 * @brief Insert a buffer at the front of the LRU list.
 * @param bp Buffer to insert.
 */
void insert_at_front(struct buf *bp) noexcept {
    bp->b_prev = NIL_BUF;
    bp->b_next = front;
    if (front) {
        front->b_prev = bp;
    } else {
        rear = bp;
    }
    front = bp;
}

/**
 * @brief Insert a buffer at the rear of the LRU list.
 * @param bp Buffer to insert.
 */
void insert_at_rear(struct buf *bp) noexcept {
    bp->b_next = NIL_BUF;
    bp->b_prev = rear;
    if (rear) {
        rear->b_next = bp;
    } else {
        front = bp;
    }
    rear = bp;
}

/**
 * @brief Remove a buffer from its hash chain.
 * @param bp Buffer to remove.
 */
void remove_from_hash(struct buf *bp) noexcept {
    auto &head = buf_hash[hash_index(bp->b_blocknr)];
    struct buf **cur = &head;
    while (*cur && *cur != bp) {
        cur = &(*cur)->b_hash;
    }
    if (*cur == bp) {
        *cur = bp->b_hash;
    }
    bp->b_hash = NIL_BUF;
}

/**
 * @brief Insert a buffer into its hash chain.
 * @param bp Buffer to insert.
 */
void insert_into_hash(struct buf *bp) noexcept {
    auto &head = buf_hash[hash_index(bp->b_blocknr)];
    bp->b_hash = head;
    head = bp;
}
} // namespace

/**
 * @brief Perform device I/O on a buffer.
 * @param bp      Buffer to operate on.
 * @param rw_flag Either ::READING or ::WRITING.
 * @return ::OK on success or an error code from ::dev_io.
 */
int rw_block(struct buf *bp, int rw_flag) {
    const long position = static_cast<long>(bp->b_blocknr) * BLOCK_SIZE;
    const int r = dev_io(rw_flag, bp->b_dev, position, BLOCK_SIZE, FS_PROC_NR, bp->b_data);
    if (r == OK && rw_flag == WRITING) {
        bp->b_dirt = CLEAN;
    }
    return r;
}

/**
 * @brief Acquire a block from the buffer cache.
 * @param dev   Device number containing the block.
 * @param block Block number on the device.
 * @param mode  I/O mode controlling on-disk reads.
 * @return Pointer to the cached buffer or ::NIL_BUF on failure.
 */
[[nodiscard]] struct buf *get_block(dev_nr dev, block_nr block, IoMode mode) {
    auto *bp = buf_hash[hash_index(block)];
    while (bp != NIL_BUF) {
        if (bp->b_blocknr == block && bp->b_dev == dev) {
            if (++bp->b_count == 1) {
                ++bufs_in_use;
            }
            remove_from_lru(bp);
            insert_at_rear(bp);
            return bp;
        }
        bp = bp->b_hash;
    }

    bp = front;
    while (bp != NIL_BUF && bp->b_count != 0) {
        bp = bp->b_next;
    }
    if (bp == NIL_BUF) {
        return NIL_BUF; /* no free buffers */
    }

    remove_from_lru(bp);
    remove_from_hash(bp);

    if (bp->b_dirt == DIRTY) {
        rw_block(bp, WRITING);
    }

    bp->b_blocknr = block;
    bp->b_dev = dev;
    bp->b_dirt = CLEAN;
    bp->b_count = 1;

    if (mode == IoMode::Normal && dev != kNoDev) {
        if (rw_block(bp, READING) != OK) {
            bp->b_dev = kNoDev;
            bp->b_blocknr = kNoBlock;
            bp->b_count = 0;
            insert_at_front(bp);
            insert_into_hash(bp);
            return NIL_BUF;
        }
    }

    insert_into_hash(bp);
    insert_at_rear(bp);
    ++bufs_in_use;
    return bp;
}

/**
 * @brief Release a buffer, optionally writing it back to disk.
 * @param bp        Buffer to release.
 * @param block_type Usage hint guiding cache eviction.
 */
void put_block(struct buf *bp, BlockType block_type) {
    if (bp == NIL_BUF) {
        return;
    }
    if (bp->b_count == 0) {
        return; /* already free */
    }
    if (--bp->b_count > 0) {
        return;
    }
    --bufs_in_use;

    if (has_flag(block_type, BlockType::WriteImmediate) && bp->b_dirt == DIRTY) {
        rw_block(bp, WRITING);
    }

    remove_from_lru(bp);
    if (has_flag(block_type, BlockType::OneShot)) {
        insert_at_front(bp);
    } else {
        insert_at_rear(bp);
    }
}

/**
 * @brief Invalidate all cache entries for a device.
 * @param dev Device whose blocks should be purged.
 */
void invalidate(dev_nr dev) {
    for (auto *bp = &buf[0]; bp < &buf[NR_BUFS]; ++bp) {
        if (bp->b_dev == dev) {
            remove_from_hash(bp);
            bp->b_dev = kNoDev;
            bp->b_blocknr = kNoBlock;
            bp->b_dirt = CLEAN;
        }
    }
}

/**
 * @brief Convenience factory creating a ::BufferGuard for a block.
 * @param dev   Device number containing the block.
 * @param block Block number on the device.
 * @param mode  I/O mode controlling on-disk reads.
 * @param type  Usage hint passed to ::put_block.
 * @return Guard managing the buffer.
 */
[[nodiscard]] BufferGuard make_buffer_guard(dev_nr dev, block_nr block, IoMode mode,
                                            BlockType type = BlockType::FullData) {
    return BufferGuard{get_block(dev, block, mode), type};
}

/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file manages the
 * cache.
 *
 * The entry points into this file are:
 *   get_block:	  request to fetch a block for reading or writing from cache
 *   put_block:	  return a block previously requested with get_block
 *   alloc_zone:  allocate a new zone (to increase the length of a file)
 *   free_zone:	  release a zone (when a file is removed)
 *   rw_block:	  read or write a block from the disk itself
 *   invalidate:  remove all the cache blocks on some device
 */

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "super.hpp"
#include "type.hpp"
#include <minix/fs/const.hpp>

using IoMode = minix::fs::DefaultFsConstants::IoMode;

/// Forward declaration needed for BufferGuard.
void put_block(struct buf *bp, int block_type);

/**
 * @class BufferGuard
 * @brief RAII helper that automatically releases a buffer using put_block.
 */
class BufferGuard {
  public:
    BufferGuard(struct buf *bp, int type) : bp_{bp}, type_{type} {}
    BufferGuard(const BufferGuard &) = delete;
    BufferGuard &operator=(const BufferGuard &) = delete;
    BufferGuard(BufferGuard &&other) noexcept : bp_{other.bp_}, type_{other.type_} {
        other.bp_ = nullptr;
    }
    BufferGuard &operator=(BufferGuard &&other) noexcept {
        if (this != &other) {
            release();
            bp_ = other.bp_;
            type_ = other.type_;
            other.bp_ = nullptr;
        }
        return *this;
    }
    ~BufferGuard() { release(); }

    [[nodiscard]] auto get() const noexcept -> struct buf * { return bp_; }
    [[nodiscard]] auto operator->() const noexcept -> struct buf * { return bp_; }
    [[nodiscard]] auto operator*() const noexcept -> struct buf & { return *bp_; }

    void release() noexcept {
        if (bp_ != nullptr) {
            put_block(bp_, type_);
            bp_ = nullptr;
        }
    }

  private:
    struct buf *bp_{nullptr};
    int type_{};
};

/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
/**
 * @brief Retrieve a block from the buffer cache.
 *
 * This function checks if the requested block is already cached. If not, a
 * buffer is chosen from the LRU list and the block is read from disk unless
 * @p mode equals IoMode::NoRead.
 *
 * @param dev  Device on which the block resides.
 * @param blk  Block number to obtain.
 * @param mode I/O mode controlling whether the block is read from disk.
 * @return Pointer to the buffer containing the block.
 */
auto get_block(dev_nr dev, block_nr blk, IoMode mode) -> struct buf * {
    /* Check to see if the requested block is in the block cache.  If so, return
     * a pointer to it.  If not, evict some other block and fetch it.  All
     * blocks in the cache, whether in use or not, are linked together in a chain
     * with 'front' pointing to the least recently used block and 'rear' to the
     * most recently used block.  When @p mode is IoMode::NoRead the buffer is
     * returned without reading from disk because the caller intends to
     * overwrite the entire block.
     * In addition to the LRU chain, there is also a hash chain to link together
     * blocks whose block numbers end with the same bit strings, for fast lookup.
     */

    register struct buf *bp, *prev_ptr;

    /* Search the list of blocks not currently in use for (dev, blk). */
    bp = buf_hash[blk & (NR_BUF_HASH - 1)]; /* search the hash chain */
    if (dev != kNoDev) {
        while (bp != NIL_BUF) {
            if (bp->b_blocknr == blk && bp->b_dev == dev) {
                /* Block needed has been found. */
                if (bp->b_count == 0)
                    bufs_in_use++;
                bp->b_count++; /* record that block is in use */
                return (bp);
            } else {
                /* This block is not the one sought. */
                bp = bp->b_hash; /* move to next block on hash chain */
            }
        }
    }

    /* Desired block is not on available chain.  Take oldest block ('front').
     * However, a block that is aready in use (b_count > 0) may not be taken.
     */
    if (bufs_in_use == NR_BUFS)
        panic("All buffers in use", NR_BUFS);
    bufs_in_use++; /* one more buffer in use now */
    bp = front;
    while (bp->b_count > 0 && bp->b_next != NIL_BUF)
        bp = bp->b_next;
    if (bp == NIL_BUF || bp->b_count > 0)
        panic("No free buffer", NO_NUM);

    /* Remove the block that was just taken from its hash chain. */
    prev_ptr = buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)];
    if (prev_ptr == bp) {
        buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)] = bp->b_hash;
    } else {
        /* The block just taken is not on the front of its hash chain. */
        while (prev_ptr->b_hash != NIL_BUF)
            if (prev_ptr->b_hash == bp) {
                prev_ptr->b_hash = bp->b_hash; /* found it */
                break;
            } else {
                prev_ptr = prev_ptr->b_hash; /* keep looking */
            }
    }

    /* If the  block taken is dirty, make it clean by rewriting it to disk. */
    if (bp->b_dirt == DIRTY && bp->b_dev != kNoDev)
        rw_block(bp, WRITING);

    /* Fill in block's parameters and add it to the hash chain where it goes. */
    bp->b_dev = dev;     /* fill in device number */
    bp->b_blocknr = blk; /* fill in block number */
    bp->b_count++;       /* record that block is being used */
    bp->b_hash = buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)];
    buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)] = bp; /* add to hash list */

    /* Go get the requested block, unless mode == IoMode::NoRead. */
    if (dev != kNoDev && mode == IoMode::Normal)
        rw_block(bp, READING);
    return (bp); /* return the newly acquired block */
}

/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
/**
 * @brief Release a buffer previously obtained with get_block.
 *
 * Depending on @p block_type the buffer is placed either at the front or the
 * rear of the LRU chain. Important buffers may be written immediately to disk.
 *
 * @param bp         Buffer to release.
 * @param block_type Block usage hint, e.g. INODE_BLOCK or DIRECTORY_BLOCK.
 */
void put_block(struct buf *bp, int block_type) {
    /* Return a block to the list of available blocks.   Depending on 'block_type'
     * it may be put on the front or rear of the LRU chain.  Blocks that are
     * expected to be needed again shortly (e.g., partially full data blocks)
     * go on the rear; blocks that are unlikely to be needed again shortly
     * (e.g., full data blocks) go on the front.  Blocks whose loss can hurt
     * the integrity of the file system (e.g., inode blocks) are written to
     * disk immediately if they are dirty.
     */

    register struct buf *next_ptr, *prev_ptr;

    if (bp == NIL_BUF)
        return; /* it is easier to check here than in caller */

    /* If block is no longer in use, first remove it from LRU chain. */
    bp->b_count--; /* there is one use fewer now */
    if (bp->b_count > 0)
        return; /* block is still in use */

    bufs_in_use--;         /* one fewer block buffers in use */
    next_ptr = bp->b_next; /* successor on LRU chain */
    prev_ptr = bp->b_prev; /* predecessor on LRU chain */
    if (prev_ptr != NIL_BUF)
        prev_ptr->b_next = next_ptr;
    else
        front = next_ptr; /* this block was at front of chain */

    if (next_ptr != NIL_BUF)
        next_ptr->b_prev = prev_ptr;
    else
        rear = prev_ptr; /* this block was at rear of chain */

    /* Put this block back on the LRU chain.  If the ONE_SHOT bit is set in
     * 'block_type', the block is not likely to be needed again shortly, so put
     * it on the front of the LRU chain where it will be the first one to be
     * taken when a free buffer is needed later.
     */
    if (block_type & ONE_SHOT) {
        /* Block probably won't be needed quickly. Put it on front of chain.
         * It will be the next block to be evicted from the cache.
         */
        bp->b_prev = NIL_BUF;
        bp->b_next = front;
        if (front == NIL_BUF)
            rear = bp; /* LRU chain was empty */
        else
            front->b_prev = bp;
        front = bp;
    } else {
        /* Block probably will be needed quickly.  Put it on rear of chain.
         * It will not be evicted from the cache for a long time.
         */
        bp->b_prev = rear;
        bp->b_next = NIL_BUF;
        if (rear == NIL_BUF)
            front = bp;
        else
            rear->b_next = bp;
        rear = bp;
    }

    /* Some blocks are so important (e.g., inodes, indirect blocks) that they
     * should be written to the disk immediately to avoid messing up the file
     * system in the event of a crash.
     */
    if ((block_type & WRITE_IMMED) && bp->b_dirt == DIRTY && bp->b_dev != kNoDev)
        rw_block(bp, WRITING);

    /* Super blocks must not be cached, lest mount use cached block. */
    if (block_type == ZUPER_BLOCK)
        bp->b_dev = kNoDev;
}

/*===========================================================================*
 *				alloc_zone				     *
 *===========================================================================*/
/**
 * @brief Allocate a new zone on a device.
 *
 * The allocation tries to obtain a zone near @p z to improve locality.
 *
 * @param dev Device on which to allocate the zone.
 * @param z   Desired zone vicinity.
 * @return Newly allocated zone number or NO_ZONE on failure.
 */
auto alloc_zone(dev_nr dev, zone_nr z) -> zone_nr {
    /* Allocate a new zone on the indicated device and return its number. */

    bit_nr b, bit;
    struct super_block *sp;
    int major, minor;
    extern bit_nr alloc_bit();
    extern struct super_block *get_super();

    /* Note that the routine alloc_bit() returns 1 for the lowest possible
     * zone, which corresponds to sp->s_firstdatazone.  To convert a value
     * between the bit number, 'b', used by alloc_bit() and the zone number, 'z',
     * stored in the inode, use the formula:
     *     z = b + sp->s_firstdatazone - 1
     * Alloc_bit() never returns 0, since this is used for NO_BIT (failure).
     */
    sp = get_super(dev); /* find the super_block for this device */
    bit = (bit_nr)z - (sp->s_firstdatazone - 1);
    b = alloc_bit(sp->s_zmap, (bit_nr)sp->s_nzones - sp->s_firstdatazone + 1, sp->s_zmap_blocks,
                  bit);
    if (b == NO_BIT) {
        err_code = ErrorCode::ENOSPC;
        major = (int)(sp->s_dev >> MAJOR) & BYTE;
        minor = (int)(sp->s_dev >> MINOR) & BYTE;
        if (sp->s_dev == ROOT_DEV)
            printf("No space on root device (RAM disk)\n");
        else
            printf("No space on device %d/%d\n", major, minor);
        return (NO_ZONE);
    }
    return (sp->s_firstdatazone - 1 + (zone_nr)b);
}

/*===========================================================================*
 *				free_zone				     *
 *===========================================================================*/
/**
 * @brief Release a previously allocated zone.
 *
 * @param dev  Device on which the zone resides.
 * @param numb Zone number to free.
 */
void free_zone(dev_nr dev, zone_nr numb) {
    /* Return a zone. */

    register struct super_block *sp;
    extern struct super_block *get_super();

    if (numb == NO_ZONE)
        return; /* checking here easier than in caller */

    /* Locate the appropriate super_block and return bit. */
    sp = get_super(dev);
    free_bit(sp->s_zmap, (bit_nr)numb - (sp->s_firstdatazone - 1));
}

/*===========================================================================*
 *				rw_block				     *
 *===========================================================================*/
/**
 * @brief Perform raw I/O on a buffer.
 *
 * @param bp      Buffer to read or write.
 * @param rw_flag Either READING or WRITING.
 */
void rw_block(struct buf *bp, int rw_flag) {
    /* Read or write a disk block. This is the only routine in which actual disk
     * I/O is invoked.  If an error occurs, a message is printed here, but the error
     * is not reported to the caller.  If the error occurred while purging a block
     * from the cache, it is not clear what the caller could do about it anyway.
     */

    int r;
    long pos;
    dev_nr dev;
    extern int rdwt_err;

    if (bp->b_dev != kNoDev) {
        pos = (long)bp->b_blocknr * BLOCK_SIZE;
        r = dev_io(rw_flag, bp->b_dev, pos, BLOCK_SIZE, FS_PROC_NR, bp->b_data);
        if (r < 0) {
            dev = bp->b_dev;
            if (r != EOF) {
                printf("Unrecoverable disk error on device %d/%d, block %d\n",
                       (dev >> MAJOR) & BYTE, (dev >> MINOR) & BYTE, bp->b_blocknr);
            } else {
                bp->b_dev = kNoDev; /* invalidate block */
            }
            rdwt_err = r; /* report error to interested parties */
        }
    }

    bp->b_dirt = CLEAN;
}

/*===========================================================================*
 *				invalidate				     *
 *===========================================================================*/
/**
 * @brief Invalidate all cached blocks belonging to a device.
 *
 * @param device Device whose cached blocks should be purged.
 */
void invalidate(dev_nr device) {
    for (auto bp = &buf[0]; bp < &buf[NR_BUFS]; ++bp) {
        if (bp->b_dev == device) {
            bp->b_dev = kNoDev;
        }
    }
}

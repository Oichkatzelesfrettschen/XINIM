#pragma once
// Modernized for C++23

/* Buffer (block) cache.  To acquire a block, a routine calls get_block(),
 * telling which block it wants.  The block is then regarded as "in use"
 * and has its 'b_count' field incremented.  All the blocks, whether in use
 * or not, are chained together in an LRU list, with 'front' pointing
 * to the least recently used block, and 'rear' to the most recently used
 * block.  A reverse chain, using the field b_prev is also maintained.
 * Usage for LRU is measured by the time the put_block() is done.  The second
 * parameter to put_block() can violate the LRU order and put a block on the
 * front of the list, if it will probably not be needed soon.  If a block
 * is modified, the modifying routine must set b_dirt to DIRTY, so the block
 * will eventually be rewritten to the disk.
 */

/**
 * @brief Descriptor for a cached disk block.
 */
EXTERN struct buf {
    /** Data portion of the buffer. */
    union {
        char b__data[BLOCK_SIZE];           /**< Ordinary user data. */
        dir_struct b__dir[NR_DIR_ENTRIES];  /**< Directory block. */
        zone_nr b__ind[NR_INDIRECTS];       /**< Indirect block. */
        d_inode b__inode[INODES_PER_BLOCK]; /**< Inode block. */
        int b__int[INTS_PER_BLOCK];         /**< Block full of integers. */
    } b;

    /** Header portion of the buffer. */
    struct buf *b_next; /**< Used to link buffers in a chain. */
    struct buf *b_prev; /**< Used to link buffers the other way. */
    struct buf *b_hash; /**< Used to link buffers on hash chains. */
    block_nr b_blocknr; /**< Block number of its (minor) device. */
    dev_nr b_dev;       /**< Major | minor device where block resides. */
    char b_dirt;        /**< CLEAN or DIRTY. */
    char b_count;       /**< Number of users of this buffer. */
} buf[NR_BUFS];

/* A block is free if b_dev == NO_DEV. */

#define NIL_BUF (struct buf *)0 /* indicates absence of a buffer */

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data b.b__data
#define b_dir b.b__dir
#define b_ind b.b__ind
#define b_inode b.b__inode
#define b_int b.b__int

/** Hash table for quick buffer lookup by block number. */
EXTERN struct buf *buf_hash[NR_BUF_HASH];

/** Points to the least recently used free block. */
EXTERN struct buf *front;

/** Points to the most recently used free block. */
EXTERN struct buf *rear;

/** Number of buffers currently in use (not on free list). */
EXTERN int bufs_in_use;

/**
 * @brief Usage hint flags and classifications for cached blocks.
 */
enum class BlockType : int {
    WriteImmediate = 0100,                /**< Block should be written to disk now. */
    OneShot = 0200,                       /**< Block not likely to be needed soon. */
    Inode = 0 + WriteImmediate,           /**< Inode block. */
    Directory = 1 + WriteImmediate,       /**< Directory block. */
    Indirect = 2 + WriteImmediate,        /**< Pointer block. */
    IMap = 3 + WriteImmediate + OneShot,  /**< Inode bit map. */
    ZMap = 4 + WriteImmediate + OneShot,  /**< Free zone map. */
    Zuper = 5 + WriteImmediate + OneShot, /**< Super block. */
    FullData = 6,                         /**< Data, fully used. */
    PartialData = 7                       /**< Data, partly used. */
};

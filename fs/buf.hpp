#pragma once
/**
 * @file buf.hpp
 * @brief Buffer (block) cache definitions.
 *
 * Buffer (block) cache.  To acquire a block, a routine calls get_block(),
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
 * @struct buf
 * @brief Cache buffer for file system blocks.
 */
EXTERN struct buf {
    /**
     * @union data_u
     * @brief Union view of the buffer's data portion.
     */
    union {
        char b__data[BLOCK_SIZE];           /**< Ordinary user data. */
        dir_struct b__dir[NR_DIR_ENTRIES];  /**< Directory block. */
        zone_nr b__ind[NR_INDIRECTS];       /**< Indirect block. */
        d_inode b__inode[INODES_PER_BLOCK]; /**< Inode block. */
        int b__int[INTS_PER_BLOCK];         /**< Block full of integers. */
    } b;                                    /**< Buffer data content. */

    /** Pointer to next buffer in the LRU chain. */
    struct buf *b_next;
    /** Pointer to previous buffer in the LRU chain. */
    struct buf *b_prev;
    /** Pointer used in hash table chains. */
    struct buf *b_hash;
    /** Block number on the device. */
    block_nr b_blocknr;
    /** Device identifier where this block resides. */
    dev_nr b_dev;
    /** Dirty flag indicating whether the buffer needs writing. */
    char b_dirt;
    /** Number of active users holding this buffer. */
    char b_count;
} buf[NR_BUFS];

/** A block is free if b_dev equals ::NO_DEV. */

/** Indicates absence of a buffer. */
#define NIL_BUF (struct buf *)0

/**
 * Shorthand macros to access the union members directly from a ::buf pointer.
 */
#define b_data b.b__data
#define b_dir b.b__dir
#define b_ind b.b__ind
#define b_inode b.b__inode
#define b_int b.b__int

/** The buffer hash table indexed by block number. */
EXTERN struct buf *buf_hash[NR_BUF_HASH];

/** Least recently used free block. */
EXTERN struct buf *front;
/** Most recently used free block. */
EXTERN struct buf *rear;
/** Number of buffers currently not on the free list. */
EXTERN int bufs_in_use;

/**
 * Enumeration of possible block types and usage hints for ::put_block().
 */
#define WRITE_IMMED 0100                       /**< Block should be written immediately. */
#define ONE_SHOT 0200                          /**< Block unlikely to be reused soon. */
#define INODE_BLOCK 0 + WRITE_IMMED            /**< Inode block. */
#define DIRECTORY_BLOCK 1 + WRITE_IMMED        /**< Directory block. */
#define INDIRECT_BLOCK 2 + WRITE_IMMED         /**< Pointer block. */
#define I_MAP_BLOCK 3 + WRITE_IMMED + ONE_SHOT /**< Inode bit map. */
#define ZMAP_BLOCK 4 + WRITE_IMMED + ONE_SHOT  /**< Free zone map. */
#define ZUPER_BLOCK 5 + WRITE_IMMED + ONE_SHOT /**< Super block. */
#define FULL_DATA_BLOCK 6                      /**< Data, fully used. */
#define PARTIAL_DATA_BLOCK 7                   /**< Data, partly used. */

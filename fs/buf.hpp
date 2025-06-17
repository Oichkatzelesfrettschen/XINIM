#pragma once
/**
 * @file buf.hpp
 * @brief Unified buffer (block) cache for file system subsystems.
 *
 * The buffer cache provides a fixed-size in-memory pool for recently used
 * blocks. Buffers are organized in an LRU chain and a fast hash table
 * for quick lookup by (device, block number).
 *
 * Blocks are marked CLEAN or DIRTY and reference counted. Each buffer
 * can represent different structures—raw data, inodes, directory entries,
 * indirect blocks—accessed via union overlay macros.
 *
 * To acquire a buffer, use `get_block()`. Release it with `put_block()`,
 * optionally providing usage hints to guide eviction policy.
 */

#include "fs/const.hpp"
#include "fs/type.hpp"
#include "fs/inode.hpp"
#include "fs/super.hpp"

/// Forward declaration of the buffer struct.
struct buf;

/**
 * @struct buf
 * @brief Represents a single cached block in the buffer cache.
 */
EXTERN struct buf {
    /**
     * @union
     * @brief Overlay union to reinterpret block contents.
     */
    union {
        char b__data[BLOCK_SIZE];              ///< Raw block data.
        dir_struct b__dir[NR_DIR_ENTRIES];     ///< Directory entries.
        zone_nr b__ind[NR_INDIRECTS];          ///< Indirect block zones.
        d_inode b__inode[INODES_PER_BLOCK];    ///< Inode table block.
        int b__int[INTS_PER_BLOCK];            ///< Integer array (e.g., bitmaps).
    } b; ///< Buffer data section.

    buf* b_next;    ///< Next buffer in LRU or hash chain.
    buf* b_prev;    ///< Previous buffer in LRU chain.
    buf* b_hash;    ///< Next in hash table chain.

    block_nr b_blocknr; ///< Block number within device.
    dev_nr b_dev;       ///< Device identifier (major | minor).
    char b_dirt;        ///< DIRTY if modified; CLEAN otherwise.
    char b_count;       ///< Number of active users holding this buffer.
} buf[NR_BUFS]; ///< Global buffer array.

/**
 * @name Buffer Pointer Macros
 * @brief Access overlayed buffer data fields from a ::buf pointer.
 */
///@{
#define b_data  b.b__data
#define b_dir   b.b__dir
#define b_ind   b.b__ind
#define b_inode b.b__inode
#define b_int   b.b__int
///@}

/** Null buffer pointer. */
#define NIL_BUF (struct buf*)0

/** A buffer is free when its device field equals NO_DEV. */

/**
 * @brief Buffer hash table indexed by block number modulo NR_BUF_HASH.
 */
EXTERN struct buf* buf_hash[NR_BUF_HASH];

/**
 * @brief Least recently used (front) and most recently used (rear) buffer pointers.
 *
 * These maintain a doubly-linked circular LRU list of all buffers.
 */
EXTERN struct buf* front; ///< LRU (least recently used) buffer.
EXTERN struct buf* rear;  ///< MRU (most recently used) buffer.

/**
 * @brief Number of buffers currently held in use (not on the free list).
 */
EXTERN int bufs_in_use;

/**
 * @enum BlockType
 * @brief Usage hints provided to `put_block()` to guide eviction policy.
 */
enum class BlockType : int {
    /// Write this block back immediately to disk.
    WriteImmediate = 0100,

    /// This block is unlikely to be reused soon.
    OneShot = 0200,

    /// This block contains inode metadata.
    Inode = 0 + WriteImmediate,

    /// This block contains directory entries.
    Directory = 1 + WriteImmediate,

    /// This block contains indirect zone pointers.
    Indirect = 2 + WriteImmediate,

    /// This block is part of the inode allocation bitmap.
    IMap = 3 + WriteImmediate + OneShot,

    /// This block is part of the zone allocation bitmap.
    ZMap = 4 + WriteImmediate + OneShot,

    /// This block contains the filesystem superblock.
    Zuper = 5 + WriteImmediate + OneShot,

    /// Full block of user data (no internal fragmentation).
    FullData = 6,

    /// Partial user data (with tail fragmentation).
    PartialData = 7
};

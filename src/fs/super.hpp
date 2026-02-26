/**
 * @file super.hpp
 * @brief Super block table for mounted file systems.
 *
 * The root file system and every mounted file system
 * has an entry here.  The entry holds information about the sizes of the bit
 * maps and inodes.  The s_ninodes field gives the number of inodes available
 * for files and directories, including the root directory.  Inode 0 is
 * on the disk, but not used.  Thus s_ninodes = 4 means that 5 bits will be
 * used in the bit map, bit 0, which is always 1 and not used, and bits 1-4
 * for files and directories.  The disk layout is:
 *
 *      Item        # blocks
 *    boot block      1
 *    super block     1
 *    inode map     s_imap_blocks
 *    zone map      s_zmap_blocks
 *    inodes        (s_ninodes + 1 + INODES_PER_BLOCK - 1)/INODES_PER_BLOCK
 *    unused        whatever is needed to fill out the current zone
 *    data zones    (s_nzones - s_firstdatazone) << s_log_zone_size
 *
 * A super_block slot is free if s_dev == NO_DEV.
 */

#pragma once

#include "const.hpp"

struct buf;
struct inode;

/**
 * @struct super_block
 * @brief In-memory super block state and on-disk metadata.
 */
EXTERN struct super_block {
    inode_nr s_ninodes;        ///< # usable inodes on the minor device.
    zone_nr s_nzones;          ///< Total device size, including bitmaps.
    unshort s_imap_blocks;     ///< # of blocks used by inode bit map.
    unshort s_zmap_blocks;     ///< # of blocks used by zone bit map.
    zone_nr s_firstdatazone;   ///< Number of first data zone.
    short int s_log_zone_size; ///< log2 of blocks/zone.
    file_pos s_max_size;       ///< Maximum file size on this device.
    file_pos64 s_max_size64;   ///< 64-bit maximum file size.
    int s_magic;               ///< Magic number to recognize super-blocks.

    /* The following items are only used when the super_block is in memory. */
    struct buf *s_imap[I_MAP_SLOTS]; ///< In-core inode bit map pointers.
    struct buf *s_zmap[ZMAP_SLOTS];  ///< In-core zone bit map pointers.
    dev_nr s_dev;                    ///< Device owning this super block.
    struct inode *s_isup;            ///< Inode for root dir of mounted file sys.
    struct inode *s_imount;          ///< Inode mounted on.
    real_time s_time;                ///< Time of last update.
    char s_rd_only;                  ///< 1 iff file system mounted read-only.
    char s_dirt;                     ///< CLEAN or DIRTY.
} super_block[NR_SUPERS];

/**
 * @brief Load inode and zone bitmaps for a device.
 */
int load_bit_maps(dev_nr dev);
/**
 * @brief Flush and unload inode/zone bitmaps for a device.
 */
int unload_bit_maps(dev_nr dev);
/**
 * @brief Allocate a free bit from a bitmap.
 */
bit_nr alloc_bit(struct buf *map_ptr[], bit_nr map_bits, unshort bit_blocks, bit_nr origin);
/**
 * @brief Release a previously allocated bit in a bitmap.
 */
void free_bit(struct buf *map_ptr[], bit_nr bit_returned);
/**
 * @brief Allocate a free zone on a device.
 */
zone_nr alloc_zone(dev_nr dev, zone_nr zone);
/**
 * @brief Free a previously allocated zone.
 */
void free_zone(dev_nr dev, zone_nr zone);
/**
 * @brief Look up the super block for a device.
 */
struct super_block *get_super(dev_nr dev);
/**
 * @brief Report whether an inode is on a mounted or root file system.
 */
int mounted(struct inode *rip);
/**
 * @brief Return the zone-to-block scale factor for an inode.
 */
int scale_factor(struct inode *ip);
/**
 * @brief Read or write a super block to disk.
 */
void rw_super(struct super_block *sp, int rw_flag);

#define NIL_SUPER (struct super_block *)0

/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   load_bit_maps:   get the bit maps for the root or a newly mounted device
 *   unload_bit_maps: write the bit maps back to disk after an UMOUNT
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 *   get_super:       search the 'superblock' table for a device
 *   mounted:         tells if file inode is on mounted (or ROOT) file system
 *   scale_factor:    get the zone-to-block conversion factor for a device
 *   rw_super:        read or write a superblock
 */

#include "super.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "buf.hpp"
#include "const.hpp"
#include "inode.hpp"
#include "type.hpp"
#include <cstddef>

#define INT_BITS (sizeof(int) << 3)
#define BIT_MAP_SHIFT 13 /* (log2 of BLOCK_SIZE) + 3; 13 for 1k blocks */

/**
 * @brief Copy a byte sequence between buffers.
 */
extern void copy(char *dest, const char *src, std::size_t length);
/**
 * @brief Acquire a block buffer from the cache.
 */
extern struct buf *get_block(dev_nr dev, block_nr block, int how);
/**
 * @brief Release a block buffer to the cache.
 */
extern void put_block(struct buf *bp, BlockType how);
/**
 * @brief Report a fatal error and halt.
 */
extern void panic(const char *s, int n);
/**
 * @brief Locate a super block for a given device.
 */
PUBLIC struct super_block *get_super(dev_nr dev);

/**
 * @brief Global error code used by FS helpers.
 */
extern int err_code;

constexpr std::size_t kSuperSize = sizeof(super_block);

/*===========================================================================*
 *				load_bit_maps				     *
 *===========================================================================*/
/**
 * @brief Load inode and zone bitmaps for a device into the buffer cache.
 * @param dev Device number to load.
 * @return OK on success or ERROR on insufficient buffers.
 */
PUBLIC int load_bit_maps(dev_nr dev) {
    /* Load the bit map for some device into the cache and set up superblock. */
    int i = 0;
    struct super_block *sp = get_super(dev);
    block_nr zbase = 0;

    if (bufs_in_use + sp->s_imap_blocks + sp->s_zmap_blocks >= NR_BUFS - 3)
        return (ERROR); /* insufficient buffers left for bit maps */
    if (sp->s_imap_blocks > I_MAP_SLOTS || sp->s_zmap_blocks > ZMAP_SLOTS)
        panic("too many map blocks", NO_NUM);

    /* Load the inode map from the disk. */
    for (i = 0; i < sp->s_imap_blocks; i++)
        sp->s_imap[i] = get_block(dev, SUPER_BLOCK + 1 + i, NORMAL);

    /* Load the zone map from the disk. */
    zbase = SUPER_BLOCK + 1 + sp->s_imap_blocks;
    for (i = 0; i < sp->s_zmap_blocks; i++)
        sp->s_zmap[i] = get_block(dev, zbase + i, NORMAL);

    /* inodes 0 and 1, and zone 0 are never allocated.  Mark them as busy. */
    sp->s_imap[0]->b_int[0] |= 3; /* inodes 0, 1 busy */
    sp->s_zmap[0]->b_int[0] |= 1; /* zone 0 busy */
    bufs_in_use += sp->s_imap_blocks + sp->s_zmap_blocks;
    return (OK);
}

/*===========================================================================*
 *				unload_bit_maps				     *
 *===========================================================================*/
/**
 * @brief Flush and unload inode/zone bitmaps for a device.
 * @param dev Device number being unmounted.
 * @return OK on success.
 */
PUBLIC int unload_bit_maps(dev_nr dev) {
    /* Unload the bit maps so a device can be unmounted. */
    int i = 0;
    struct super_block *sp = get_super(dev);

    bufs_in_use -= sp->s_imap_blocks + sp->s_zmap_blocks;
    for (i = 0; i < sp->s_imap_blocks; i++)
        put_block(sp->s_imap[i], BlockType::IMap);
    for (i = 0; i < sp->s_zmap_blocks; i++)
        put_block(sp->s_zmap[i], BlockType::ZMap);
    return (OK);
}

/*===========================================================================*
 *				alloc_bit				     *
 *===========================================================================*/
/**
 * @brief Allocate a free bit from a bitmap.
 * @param map_ptr Array of bitmap block pointers.
 * @param map_bits Total number of bits in the map.
 * @param bit_blocks Number of blocks in the bitmap.
 * @param origin Starting bit index to search from.
 * @return Allocated bit number or NO_BIT on failure.
 */
PUBLIC bit_nr alloc_bit(struct buf *map_ptr[], bit_nr map_bits, unshort bit_blocks, bit_nr origin) {
    /* Allocate a bit from a bit map and return its bit number. */
    unsigned k = 0;
    int *wptr = nullptr;
    int *wlim = nullptr;
    int i = 0;
    int a = 0;
    int b = 0;
    int w = 0;
    int o = 0;
    int block_count = 0;
    struct buf *bp = nullptr;

    /* Figure out where to start the bit search (depends on 'origin'). */
    if (origin >= map_bits)
        origin = 0; /* for robustness */
    b = origin >> BIT_MAP_SHIFT;
    o = origin - (b << BIT_MAP_SHIFT);
    w = o / INT_BITS;
    block_count = (w == 0 ? bit_blocks : bit_blocks + 1);

    /* The outer while loop iterates on the blocks of the map.  The inner
     * while loop iterates on the words of a block.  The for loop iterates
     * on the bits of a word.
     */
    while (block_count--) {
        /* If need be, loop on all the blocks in the bit map. */
        bp = map_ptr[b];
        wptr = &bp->b_int[w];
        wlim = &bp->b_int[INTS_PER_BLOCK];
        while (wptr != wlim) {
            /* Loop on all the words of one of the bit map blocks. */
            if ((k = (unsigned)*wptr) != (unsigned)~0) {
                /* This word contains a free bit.  Allocate it. */
                for (i = 0; i < INT_BITS; i++)
                    if (((k >> i) & 1) == 0) {
                        a = i + (wptr - &bp->b_int[0]) * INT_BITS + (b << BIT_MAP_SHIFT);
                        /* If 'a' beyond map check other blks*/
                        if (a >= map_bits) {
                            wptr = wlim - 1;
                            break;
                        }
                        *wptr |= 1 << i;
                        bp->b_dirt = DIRTY;
                        return ((bit_nr)a);
                    }
            }
            wptr++; /* examine next word in this bit map block */
        }
        if (++b == bit_blocks)
            b = 0; /* we have wrapped around */
        w = 0;
    }
    return (NO_BIT); /* no bit could be allocated */
}

/*===========================================================================*
 *				free_bit				     *
 *===========================================================================*/
/**
 * @brief Release a previously allocated bit in a bitmap.
 * @param map_ptr Array of bitmap block pointers.
 * @param bit_returned Bit number to release.
 */
PUBLIC void free_bit(struct buf *map_ptr[], bit_nr bit_returned) {
    /* Return a zone or inode by turning on its bitmap bit. */
    int b = 0;
    int r = 0;
    int w = 0;
    int bit = 0;
    struct buf *bp = nullptr;

    b = bit_returned >> BIT_MAP_SHIFT; /* 'b' tells which block it is in */
    r = bit_returned - (b << BIT_MAP_SHIFT);
    w = r / INT_BITS; /* 'w' tells which word it is in */
    bit = r % INT_BITS;
    bp = map_ptr[b];
    if (bp == NIL_BUF) {
        return;
    }
    if (((bp->b_int[w] >> bit) & 1) == 0)
        panic("freeing unused block or inode--check file sys", (int)bit_returned);
    bp->b_int[w] &= ~(1 << bit); /* turn the bit on */
    bp->b_dirt = DIRTY;
}

/*===========================================================================*
 *				alloc_zone				     *
 *===========================================================================*/
/**
 * @brief Allocate a free zone on a device.
 * @param dev Device to allocate from.
 * @param z Preferred starting zone (hint).
 * @return Allocated zone number or NO_ZONE on failure.
 */
PUBLIC zone_nr alloc_zone(dev_nr dev, zone_nr z) {
    struct super_block *sp = get_super(dev);
    const bit_nr origin = static_cast<bit_nr>(z);
    const bit_nr bit = alloc_bit(sp->s_zmap, static_cast<bit_nr>(sp->s_nzones),
                                 sp->s_zmap_blocks, origin);
    if (bit == NO_BIT) {
        err_code = static_cast<int>(ErrorCode::ENOSPC);
        return NO_ZONE;
    }
    if (bit < sp->s_firstdatazone || bit >= sp->s_nzones) {
        free_bit(sp->s_zmap, bit);
        err_code = static_cast<int>(ErrorCode::ENOSPC);
        return NO_ZONE;
    }
    return static_cast<zone_nr>(bit);
}

/*===========================================================================*
 *				free_zone				     *
 *===========================================================================*/
/**
 * @brief Free a previously allocated zone.
 * @param dev Device owning the zone.
 * @param zone Zone number to release.
 */
PUBLIC void free_zone(dev_nr dev, zone_nr zone) {
    if (zone == NO_ZONE) {
        return;
    }
    struct super_block *sp = get_super(dev);
    if (zone < sp->s_firstdatazone || zone >= sp->s_nzones) {
        panic("freeing invalid zone", static_cast<int>(zone));
        return;
    }
    free_bit(sp->s_zmap, static_cast<bit_nr>(zone));
}

/*===========================================================================*
 *				get_super				     *
 *===========================================================================*/
/**
 * @brief Look up the super block for a device.
 * @param dev Device number whose super block is requested.
 * @return Pointer to the super block (panics if not found).
 */
PUBLIC struct super_block *get_super(dev_nr dev) {
    /* Search the superblock table for this device.  It is supposed to be there. */
    struct super_block *sp = nullptr;

    for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++) {
        if (sp->s_dev == dev)
            return (sp);
    }

    /* Search failed.  Something wrong. */
    panic("can't find superblock for device (in decimal)", (int)dev);
}

/*===========================================================================*
 *				mounted					     *
 *===========================================================================*/
/**
 * @brief Check whether an inode resides on a mounted or root file system.
 * @param rip Pointer to inode to inspect.
 * @return TRUE if mounted or root, FALSE otherwise.
 */
PUBLIC int mounted(struct inode *rip) {
    /* Report on whether the given inode is on a mounted (or ROOT) file system. */
    struct super_block *sp = nullptr;
    dev_nr dev = static_cast<dev_nr>(0);

    dev = (dev_nr)rip->i_zone[0];
    if (dev == ROOT_DEV)
        return (TRUE); /* inode is on root file system */

    for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
        if (sp->s_dev == dev)
            return (TRUE);

    return (FALSE);
}

/*===========================================================================*
 *				scale_factor				     *
 *===========================================================================*/
/**
 * @brief Return the zone-to-block conversion factor for an inode's device.
 * @param ip Pointer to inode whose superblock is needed.
 * @return The log2 zone size (blocks per zone).
 */
PUBLIC int scale_factor(struct inode *ip) {
    /* Return the scale factor used for converting blocks to zones. */
    struct super_block *sp = get_super(ip->i_dev);
    return (sp->s_log_zone_size);
}

/*===========================================================================*
 *				rw_super				     *
 *===========================================================================*/
/**
 * @brief Read or write a super block to disk.
 * @param sp Pointer to the super block.
 * @param rw_flag READING or WRITING.
 */
PUBLIC void rw_super(struct super_block *sp, int rw_flag) {
    /* Read or write a superblock. */
    struct buf *bp = nullptr;
    dev_nr dev = static_cast<dev_nr>(0);

    /* Check if this is a read or write, and do it. */
    if (rw_flag == READING) {
        dev = sp->s_dev; /* save device; it will be overwritten by copy*/
        bp = get_block(sp->s_dev, static_cast<block_nr>(SUPER_BLOCK), NORMAL);
        copy(reinterpret_cast<char *>(sp), bp->b_data, kSuperSize);
        sp->s_dev = dev; /* restore device number */
    } else {
        /* On a write, it is not necessary to go read superblock from disk. */
        bp = get_block(sp->s_dev, static_cast<block_nr>(SUPER_BLOCK), NO_READ);
        copy(bp->b_data, reinterpret_cast<const char *>(sp), kSuperSize);
        bp->b_dirt = DIRTY;
    }

    sp->s_dirt = CLEAN;
    put_block(bp, BlockType::Zuper);
}

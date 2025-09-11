// Modernized for C++23

/* This file is the counterpart of "read.cpp".  It contains the code for writing
 * insofar as this is not contained in read_write().
 *
 * The entry points into this file are
 *   do_write:     call read_write to perform the WRITE system call
 *   write_map:    add a new zone to an inode
 *   clear_zone:   erase a zone in the middle of a file
 *   new_block:    acquire a new block
 */

#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "buf.hpp"
#include "compat.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "super.hpp"
#include "type.hpp"
#include <minix/fs/const.hpp>

using IoMode = minix::fs::DefaultFsConstants::IoMode;
#include <cstddef> // For std::size_t, nullptr
#include <cstdint> // For uint16_t, int32_t, uint32_t, int64_t

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
PUBLIC int do_write() {
    /* Perform the write(fd, buffer, nbytes) system call. */
    return (read_write(WRITING)); // read_write is in fs/read.cpp, assumed modernized
}

/*===========================================================================*
 *				write_map				     *
 *===========================================================================*/
// Modernized signature, PRIVATE -> static
static int write_map(struct inode *rip, int32_t position, uint16_t new_zone) {
    // rip is struct inode*
    // position is file_pos (int32_t)
    // new_zone is zone_nr (uint16_t)
    /* Write a new zone into an inode. */
    int scale;
    uint16_t z;           // zone_nr -> uint16_t
    uint16_t *zp;         // zone_nr* -> uint16_t*
    uint16_t b;           // block_nr -> uint16_t
    int32_t excess, zone; // Were long, derived from position (int32_t)
    int index;
    struct buf *bp;
    int new_ind, new_dbl;

    extern zone_nr alloc_zone();
    extern struct buf *get_block();
    extern real_time clock_time();

    rip->i_dirt = DIRTY;                     /* inode will be changed */
    bp = NIL_BUF;                            // NIL_BUF is (struct buf*)nullptr
    scale = scale_factor(rip);               /* for zone-block conversion (returns int) */
    zone = (position / BLOCK_SIZE) >> scale; /* position is int32_t, zone is int32_t */

    /* Is 'position' to be found in the inode itself? */
    if (zone < NR_DZONE_NUM) { // NR_DZONE_NUM is int
        // rip->i_zone is uint16_t[], new_zone is uint16_t. zone is int32_t (small index).
        rip->i_zone[static_cast<std::size_t>(zone)] = new_zone;
        rip->i_modtime = clock_time(); // clock_time returns real_time (int64_t)
        return (OK);
    }

    /* It is not in the inode, so it must be single or double indirect. */
    excess = zone - NR_DZONE_NUM; /* first NR_DZONE_NUM don't count (excess is int32_t) */
    new_ind = FALSE;
    new_dbl = FALSE;

    if (excess < static_cast<int32_t>(NR_INDIRECTS)) { // NR_INDIRECTS is std::size_t (was int)
        /* 'position' can be located via the single indirect block. */
        zp = &rip->i_zone[NR_DZONE_NUM]; // zp is uint16_t*
    } else {
        /* 'position' can be located via the double indirect block. */
        // z is uint16_t. rip->i_zone[] is uint16_t. kNoZone is uint16_t.
        if ((z = rip->i_zone[NR_DZONE_NUM + 1]) == kNoZone) {
            /* Create the double indirect block. */
            // rip->i_dev is dev_nr (uint16_t). rip->i_zone[0] is zone_nr (uint16_t).
            // alloc_zone returns zone_nr (uint16_t).
            if ((z = alloc_zone(rip->i_dev, rip->i_zone[0])) == kNoZone)
                return (err_code);
            rip->i_zone[NR_DZONE_NUM + 1] = z;
            new_dbl = TRUE; /* set flag for later */
        }

        /* Either way, 'z' is zone number for double indirect block. */
        excess -= static_cast<int32_t>(NR_INDIRECTS); /* single indirect doesn't count */
        index = static_cast<int>(excess / static_cast<int32_t>(NR_INDIRECTS)); // index is int
        excess = excess % static_cast<int32_t>(NR_INDIRECTS);
        if (index >= static_cast<int>(NR_INDIRECTS))
            return (ErrorCode::EFBIG);
        // z is uint16_t, b is uint16_t. scale is int.
        b = static_cast<uint16_t>(static_cast<uint32_t>(z) << scale);
        // rip->i_dev is dev_nr (uint16_t), b is block_nr (uint16_t).
        bp = get_block(rip->i_dev, b, (new_dbl ? IoMode::NoRead : IoMode::Normal));
        if (new_dbl)
            zero_block(bp);
        zp = &bp->b_ind[index]; // bp->b_ind is zone_nr[] (uint16_t[])
    }

    /* 'zp' now points to place where indirect zone # goes; 'excess' is index. */
    if (*zp == kNoZone) { // zp is uint16_t*, kNoZone is uint16_t
        /* Create indirect block. */
        *zp = alloc_zone(rip->i_dev, rip->i_zone[0]);
        new_ind = TRUE;
        if (bp != NIL_BUF)      // NIL_BUF is (struct buf*)nullptr
            bp->b_dirt = DIRTY; /* if double ind, it is dirty */
        if (*zp == kNoZone) {
            put_block(bp, BlockType::Indirect); /* release dbl indirect blk */
            return (err_code);                  /* couldn't create single ind */
        }
    }
    put_block(bp, BlockType::Indirect); /* release double indirect blk */

    /* 'zp' now points to indirect block's zone number. */
    // *zp is uint16_t, b is uint16_t, scale is int
    b = static_cast<uint16_t>(static_cast<uint32_t>(*zp) << scale);
    bp = get_block(rip->i_dev, b, (new_ind ? IoMode::NoRead : IoMode::Normal));
    if (new_ind)
        zero_block(bp);
    // bp->b_ind is uint16_t[]. excess is int32_t (small index). new_zone is uint16_t.
    bp->b_ind[static_cast<std::size_t>(excess)] = new_zone;
    rip->i_modtime = clock_time(); // real_time (int64_t)
    bp->b_dirt = DIRTY;
    put_block(bp, BlockType::Indirect);

    return (OK);
}

/*===========================================================================*
 *				clear_zone				     *
 *===========================================================================*/
// Modernized signature
PUBLIC void clear_zone(struct inode *rip, int32_t pos, int flag) {
    // rip is struct inode*
    // pos is file_pos (int32_t)
    // flag is int
    /* Zero a zone, possibly starting in the middle.  The parameter 'pos' gives
     * a byte in the first block to be zeroed.  Clearzone() is called from
     * read_write and new_block().
     */

    register struct buf *bp;
    uint16_t b, blo, bhi; // block_nr -> uint16_t
    int32_t next;         // file_pos -> int32_t
    int scale;            // scale_factor returns int
    uint32_t zone_size;   // zone_type -> uint32_t
    extern struct buf *get_block();
    extern uint16_t read_map(struct inode * rip, int32_t position); // Modernized read_map

    /* If the block size and zone size are the same, clear_zone() not needed. */
    if ((scale = scale_factor(rip)) == 0)
        return;

    zone_size = static_cast<uint32_t>(BLOCK_SIZE) << scale; // BLOCK_SIZE is int
    if (flag == 1) {
        // Assuming pos is non-negative for this division with uint32_t
        pos = static_cast<int32_t>((static_cast<uint32_t>(pos) / zone_size) * zone_size);
    }
    next = pos + BLOCK_SIZE - 1; // pos is int32_t, BLOCK_SIZE is int

    /* If 'pos' is in the last block of a zone, do not clear the zone. */
    // Assuming pos is non-negative for division with uint32_t
    if (static_cast<uint32_t>(next) / zone_size != static_cast<uint32_t>(pos) / zone_size)
        return;
    if ((blo = read_map(rip, next)) == kNoBlock) // kNoBlock is block_nr (uint16_t)
        return;
    // blo is uint16_t, scale is int. bhi is uint16_t.
    bhi = static_cast<uint16_t>(((static_cast<uint32_t>(blo) >> scale) + 1) << scale) - 1;

    /* Clear all the blocks between 'blo' and 'bhi'. */
    for (b = blo; b <= bhi; b++) { // b, blo, bhi are uint16_t
        // rip->i_dev is dev_nr (uint16_t)
        bp = get_block(rip->i_dev, b, IoMode::NoRead);
        zero_block(bp);
        put_block(bp, BlockType::FullData);
    }
}

/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
// Modernized signature
PUBLIC struct buf *new_block(struct inode *rip, int32_t position) {
    // rip is struct inode*
    // position is file_pos (int32_t)
    /* Acquire a new block and return a pointer to it.  Doing so may require
     * allocating a complete zone, and then returning the initial block.
     * On the other hand, the current zone may still have some unused blocks.
     */

    register struct buf *bp;
    uint16_t b, base_block; // block_nr -> uint16_t
    uint16_t z;             // zone_nr -> uint16_t
    uint32_t zone_size;     // zone_type -> uint32_t
    int scale, r;
    struct super_block *sp;
    extern struct buf *get_block();
    extern struct super_block *get_super();
    extern uint16_t read_map(struct inode * rip, int32_t position); // Modernized read_map
    extern uint16_t alloc_zone(uint16_t dev, uint16_t z);           // Modernized alloc_zone

    /* Is another block available in the current zone? */
    if ((b = read_map(rip, position)) == kNoBlock) { // kNoBlock is block_nr (uint16_t)
        /* Choose first zone if need be. */
        if (compat_get_size(rip) == 0) { // compat_get_size returns file_pos64 (int64_t)
            sp = get_super(rip->i_dev);  // rip->i_dev is dev_nr (uint16_t)
            z = sp->s_firstdatazone;     // s_firstdatazone is zone_nr (uint16_t)
        } else {
            z = rip->i_zone[0]; // i_zone[0] is zone_nr (uint16_t)
        }
        // alloc_zone takes (dev_nr, zone_nr) returns zone_nr (all uint16_t)
        if ((z = alloc_zone(rip->i_dev, z)) == kNoZone) // kNoZone is zone_nr (uint16_t)
            return (NIL_BUF);                           // NIL_BUF is (struct buf*)nullptr
        // write_map takes (inode*, file_pos, zone_nr) -> (inode*, int32_t, uint16_t)
        if ((r = write_map(rip, position, z)) != OK) {
            free_zone(rip->i_dev, z); // free_zone takes (dev_nr, zone_nr)
            err_code = r;
            return (NIL_BUF);
        }

        /* If we are not writing at EOF, clear the zone, just to be safe. */
        // position is int32_t, compat_get_size is int64_t
        if (static_cast<int64_t>(position) != compat_get_size(rip))
            clear_zone(rip, position, 1); // clear_zone takes (inode*, file_pos, int)
        scale = scale_factor(rip);        // returns int
        base_block = static_cast<uint16_t>(static_cast<uint32_t>(z) << scale); // z is uint16_t
        zone_size = static_cast<uint32_t>(BLOCK_SIZE) << scale;                // BLOCK_SIZE is int
        // position is int32_t, zone_size is uint32_t, BLOCK_SIZE is int. b is uint16_t.
        // Assuming positive position for modulo.
        b = base_block +
            static_cast<uint16_t>((static_cast<uint32_t>(position) % zone_size) / BLOCK_SIZE);
    }

    bp = get_block(rip->i_dev, b, IoMode::NoRead); // rip->i_dev is dev_nr, b is block_nr
    zero_block(bp);
    return (bp);
}

/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
// Modernized signature
PUBLIC void zero_block(struct buf *bp) { // Assuming void return
    /* Zero a block. */

    register int n;
    register int *zip; // Assuming b_int is int[] or similar

    n = INTS_PER_BLOCK; /* number of integers in a block (int const) */
    zip = bp->b_int;    /* where to start clearing */

    do {
        *zip++ = 0;
    } while (--n);
    bp->b_dirt = DIRTY;
}

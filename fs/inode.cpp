/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* This file manages the inode table.  There are procedures to allocate and
 * deallocate inodes, acquire, erase, and release them, and read and write
 * them from the disk.
 *
 * The entry points into this file are
 *   get_inode:	  search inode table for a given inode; if not there, read it
 *   put_inode:	  indicate that an inode is no longer needed in memory
 *   alloc_inode: allocate a new, unused inode
 *   wipe_inode:  erase some fields of a newly allocated inode
 *   free_inode:  mark an inode as available for a new file
 *   rw_inode:	  read a disk block and extract an inode, or corresp. write
 *   dup_inode:	  indicate that someone else is using an inode table entry
 */

#include "inode.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "compat.hpp"
#include "const.hpp"
#include "extent.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "super.hpp"
#include "type.hpp"
#include <cstddef> // For std::size_t, nullptr
#include <cstdint> // For uint16_t, uint8_t, int16_t, int32_t, int64_t

/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
PUBLIC struct inode *get_inode(uint16_t dev,
                               uint16_t numb) { // dev_nr -> uint16_t, inode_nr -> uint16_t
    /* Find a slot in the inode table, load the specified inode into it, and
     * return a pointer to the slot.  If 'dev' == kNoDev, just return a free slot.
     */

    register struct inode *rip, *xp;

    /* Search the inode table both for (dev, numb) and a free slot. */
    xp = nullptr; // NIL_INODE -> nullptr
    for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++) {
        if (rip->i_count > 0) { /* only check used slots for (dev, numb) */
            if (rip->i_dev == dev && rip->i_num == numb) {
                /* This is the inode that we are looking for. */
                rip->i_count++;
                return (rip); /* (dev, numb) found */
            }
        } else
            xp = rip; /* remember this free slot for later */
    }

    /* Inode we want is not currently in use.  Did we find a free slot? */
    if (xp == nullptr) { /* inode table completely full */ // NIL_INODE -> nullptr
        err_code = ErrorCode::ENFILE;
        return (nullptr); // NIL_INODE -> nullptr
    }

    /* A free inode slot has been located.  Load the inode into it. */
    xp->i_dev = dev;
    xp->i_num = numb;
    xp->i_count = 1;
    if (dev != kNoDev)
        rw_inode(xp, READING); /* get inode from disk */

    return (xp);
}

/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
PUBLIC void put_inode(struct inode *rip) { // Added void return, modernized params
    /* The caller is no longer using this inode.  If no one else is using it
     * either write it back to the disk immediately.  If it has no links, truncate
     * it and return it to the pool of available inodes.
     */

    if (rip == nullptr)        // NIL_INODE -> nullptr
        return;                /* checking here is easier than in caller */
    if (--rip->i_count == 0) { /* i_count == 0 means no one is using it now */
        // rip->i_nlinks is links (uint8_t), BYTE is int. Cast to ensure consistent comparison.
        if ((static_cast<int>(rip->i_nlinks) & BYTE) == 0) {
            /* i_nlinks == 0 means free the inode. */
            truncate(rip);             /* return all the disk blocks */
            rip->i_mode = I_NOT_ALLOC; /* clear I_TYPE field */
            rip->i_pipe = NO_PIPE;
            free_inode(rip->i_dev, rip->i_num);
        }

        if (rip->i_dirt == DIRTY)
            rw_inode(rip, WRITING);
    }
}

/*===========================================================================*
 *				alloc_inode				     *
 *===========================================================================*/
PUBLIC struct inode *alloc_inode(uint16_t dev,
                                 uint16_t bits) { // dev_nr -> uint16_t, mask_bits -> uint16_t
    /* Allocate a free inode on 'dev', and return a pointer to it. */

    register struct inode *rip;
    register struct super_block *sp;
    int major, minor; // For printf, int is fine
    uint16_t numb;    // inode_nr -> uint16_t
    uint16_t b;       // bit_nr -> uint16_t
    extern bit_nr alloc_bit();
    extern struct inode *get_inode();
    extern struct super_block *get_super();

    /* Acquire an inode from the bit map. */
    sp = get_super(dev); /* get pointer to super_block */
    // sp->s_ninodes is likely uint16_t or uint32_t. Cast to bit_nr (uint16_t) is explicit.
    b = alloc_bit(sp->s_imap, static_cast<uint16_t>(sp->s_ninodes + 1), sp->s_imap_blocks,
                  static_cast<uint16_t>(0));
    if (b == NO_BIT) { // NO_BIT is uint16_t
        err_code = ErrorCode::ENFILE;
        major = static_cast<int>(static_cast<uint16_t>(sp->s_dev >> MAJOR) &
                                 BYTE); // sp->s_dev is dev_nr (uint16_t)
        minor = static_cast<int>(static_cast<uint16_t>(sp->s_dev >> MINOR) & BYTE);
        if (sp->s_dev == ROOT_DEV) // ROOT_DEV is dev_nr (uint16_t)
            printf("Out of i-nodes on root device (RAM disk)\n");
        else
            printf("Out of i-nodes on device %d/%d\n", major, minor);
        return (nullptr); // NIL_INODE -> nullptr
    }
    numb = static_cast<uint16_t>(b); // inode_nr -> uint16_t, b is bit_nr (uint16_t)

    /* Try to acquire a slot in the inode table. */
    if ((rip = get_inode(kNoDev, numb)) ==
        nullptr) { // kNoDev is dev_nr (uint16_t), NIL_INODE -> nullptr
        /* No inode table slots available.  Free the inode just allocated. */
        free_bit(sp->s_imap, b);
    } else {
        /* An inode slot is available.  Put the inode just allocated into it. */
        rip->i_mode = bits;                                     // bits is uint16_t (mask_bits)
        rip->i_nlinks = static_cast<links>(0);                  // links is uint8_t
        rip->i_uid = fp->fp_effuid;                             // uid is uint16_t
        rip->i_gid = fp->fp_effgid;                             // gid is uint8_t
        rip->i_dev = dev; /* was provisionally set to kNoDev */ // dev is uint16_t

        /* The fields not cleared already are cleared in wipe_inode().  They have
         * been put there because truncate() needs to clear the same fields if
         * the file happens to be open while being truncated.  It saves space
         * not to repeat the code twice.
         */
        wipe_inode(rip);
        init_extended_inode(rip);
    }

    return (rip);
}

/*===========================================================================*
 *				wipe_inode				     *
 *===========================================================================*/
PUBLIC void wipe_inode(struct inode *rip) { // Added void return, modernized params
    /* Erase some fields in the inode.  This function is called from alloc_inode()
     * when a new inode is to be allocated, and from truncate(), when an existing
     * inode is to be truncated.
     */

    register int i;
    extern real_time clock_time(); // clock_time returns int64_t (real_time)

    rip->i_size = 0;               // file_pos (int32_t)
    rip->i_size64 = 0;             // file_pos64 (int64_t)
    rip->i_extents = NIL_EXTENT;   // NIL_EXTENT is extent* (nullptr or specific value)
    rip->i_extent_count = 0;       // uint16_t
    rip->i_modtime = clock_time(); // real_time (int64_t)
    rip->i_dirt = DIRTY;
    for (i = 0; i < NR_ZONE_NUMS; i++)
        rip->i_zone[i] = NO_ZONE; // NO_ZONE is zone_nr (uint16_t)
}

/*===========================================================================*
 *				free_inode				     *
 *===========================================================================*/
PUBLIC void
free_inode(uint16_t dev,
           uint16_t numb) { // dev_nr -> uint16_t, inode_nr -> uint16_t. Added void return.
    /* Return an inode to the pool of unallocated inodes. */

    register struct super_block *sp;
    extern struct super_block *get_super();

    /* Locate the appropriate super_block. */
    sp = get_super(dev);
    free_bit(sp->s_imap,
             static_cast<uint16_t>(numb)); // numb is inode_nr (uint16_t), cast to bit_nr (uint16_t)
}

/*===========================================================================*
 *				rw_inode				     *
 *===========================================================================*/
PUBLIC void rw_inode(struct inode *rip, int rw_flag) { // Added void return, modernized params
    /* An entry in the inode table is to be copied to or from the disk. */

    register struct buf *bp;
    register d_inode *dip; // d_inode is struct from fs/type.hpp, members use modernized types
    register struct super_block *sp;
    uint16_t b; // block_nr -> uint16_t
    extern struct buf *get_block();
    extern struct super_block *get_super();

    /* Get the block where the inode resides. */
    sp = get_super(rip->i_dev); // rip->i_dev is dev_nr (uint16_t)
    // rip->i_num is inode_nr (uint16_t). INODES_PER_BLOCK, s_imap_blocks etc are int/size_t.
    // Result of calculation should fit block_nr (uint16_t).
    b = static_cast<uint16_t>((static_cast<uint32_t>(rip->i_num - 1) / INODES_PER_BLOCK) +
                              sp->s_imap_blocks + sp->s_zmap_blocks + 2);
    bp = get_block(rip->i_dev, b, NORMAL); // b is block_nr (uint16_t)
    // Assuming bp->b_inode is d_inode*. rip->i_num is inode_nr (uint16_t).
    dip = bp->b_inode + (rip->i_num - 1) % INODES_PER_BLOCK;

    /* Do the read or write. */
    if (rw_flag == READING) {
        copy((char *)rip, (char *)dip, INODE_SIZE); /* copy from blk to inode */
    } else {
        copy((char *)dip, (char *)rip, INODE_SIZE); /* copy from inode to blk */
        bp->b_dirt = DIRTY;
    }

    put_block(bp, INODE_BLOCK);
    rip->i_dirt = CLEAN;
}

/*===========================================================================*
 *				dup_inode				     *
 *===========================================================================*/
PUBLIC void dup_inode(struct inode *ip) { // Added void return
    /* This routine is a simplified form of get_inode() for the case where
     * the inode pointer is already known.
     */

    ip->i_count++; // i_count is int16_t
}

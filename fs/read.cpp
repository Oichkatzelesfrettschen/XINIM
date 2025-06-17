// Modernized for C++23

/* This file contains the heart of the mechanism used to read (and write)
 * files.  Read and write requests are split up into chunks that do not cross
 * block boundaries.  Each chunk is then processed in turn.  Reads on special
 * files are also detected and handled.
 *
 * The entry points into this file are
 *   do_read:	 perform the READ system call by calling read_write
 *   read_write: actually do the work of READ and WRITE
 *   read_map:	 given an inode and file position, lookup its zone number
 *   rw_user:	 call the kernel to read and write user space
 *   read_ahead: manage the block read ahead business
 */

#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "compat.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"
#include "super.hpp"
#include "type.hpp"
#include <minix/fs/const.hpp>

using IoMode = minix::fs::DefaultFsConstants::IoMode;
#include <algorithm> // For std::min
#include <cstddef>   // For std::size_t
#include <cstdint>   // For uint16_t, int32_t, int64_t etc.

#define FD_MASK 077 /* max file descriptor is 63 */

PRIVATE message umess; /* message for asking SYSTASK for user copy */
PUBLIC int rdwt_err;   /* set to ErrorCode::EIO if disk error occurs */

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
PUBLIC int do_read() { return (read_write(READING)); }

/*===========================================================================*
 *				read_write				     *
 *===========================================================================*/
PUBLIC int read_write(int rw_flag) { // Modernized signature
    /* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */

    register struct inode *rip;
    register struct filp *f;
    int32_t bytes_left;      // file_pos -> int32_t
    int64_t f_size;          // To match compat_get_size, was file_pos
    std::size_t off, cum_io; // Was unsigned
    int32_t position;        // file_pos -> int32_t
    int r;
    std::size_t chunk;             // Was int, for bytes count
    int virg, mode_word, usr, seg; // virg is bool-like
    struct filp *wf;
    extern struct super_block *get_super();
    extern struct filp *find_filp(), *get_filp();
    extern real_time clock_time();

    /* MM loads segments by putting funny things in upper 10 bits of 'fd'. */
    if (who == MM_PROC_NR && (fd & (~BYTE))) {
        usr = (fd >> 8) & BYTE;
        seg = (fd >> 6) & 03;
        fd &= FD_MASK; /* get rid of user and segment bits */
    } else {
        usr = who; /* normal case */
        seg = D;
    }

    /* If the file descriptor is valid, get the inode, size and mode. */
    if (nbytes == 0)
        return (0);                      /* so char special files need not check for 0*/
    if (who != MM_PROC_NR && nbytes < 0) // nbytes is int from message
        return (ErrorCode::EINVAL);      /* only MM > 32K */
    if ((f = get_filp(fd)) == nullptr)   // NIL_FILP -> nullptr
        return (err_code);
    if (((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) ==
        0) // filp_mode is mask_bits (uint16_t)
        return (ErrorCode::EBADF);
    position = f->filp_pos; // filp_pos is file_pos (int32_t)
    if (position < 0)       // Compare with 0 directly
        return (ErrorCode::EINVAL);
    rip = f->filp_ino;
    f_size = compat_get_size(rip); // returns file_pos64 (int64_t)
    r = OK;
    cum_io = 0; // std::size_t
    virg = TRUE;
    mode_word = rip->i_mode & I_TYPE;
    if (mode_word == I_BLOCK_SPECIAL && f_size == 0)
        f_size = MAX_P_LONG;
    rdwt_err = OK; /* set to ErrorCode::EIO if disk error occurs */

    /* Check for character special files. */
    if (mode_word == I_CHAR_SPECIAL) { // I_CHAR_SPECIAL is int
        // dev_io expects dev_nr, long, int, int, char*
        // rip->i_zone[0] is zone_nr (uint16_t). position is int32_t. nbytes is int. buffer is
        // char*.
        if ((r = dev_io(rw_flag, static_cast<uint16_t>(rip->i_zone[0]), static_cast<long>(position),
                        nbytes, who, buffer)) >= 0) {
            cum_io = static_cast<std::size_t>(r);
            position += r; // position is int32_t
            r = OK;
        }
    } else {
        if (rw_flag == WRITING && mode_word != I_BLOCK_SPECIAL) {
            /* Check in advance to see if file will grow too big. */
            // position is int32_t, s_max_size is file_pos (int32_t), nbytes is int
            if (position > get_super(rip->i_dev)->s_max_size - nbytes)
                return (ErrorCode::EFBIG);

            /* Clear the zone containing present EOF if hole about
             * to be created.  This is necessary because all unwritten
             * blocks prior to the EOF must read as zeros.
             */
            if (position > f_size) // f_size is int64_t, position is int32_t
                clear_zone(rip, static_cast<int32_t>(f_size),
                           0); // clear_zone expects file_pos (int32_t)
        }

        /* Pipes are a little different.  Check. */
        // nbytes is int. position is int32_t*.
        if (rip->i_pipe && (r = pipe_check(rip, rw_flag, virg, nbytes, &position)) <= 0)
            return (r);

        /* Split the transfer into chunks that don't span two blocks. */
        while (nbytes != 0) {
            off = static_cast<std::size_t>(
                position %
                BLOCK_SIZE); // position is int32_t, BLOCK_SIZE is int. off is std::size_t.
            // nbytes is int, BLOCK_SIZE is int, off is std::size_t. chunk is std::size_t.
            chunk = std::min(static_cast<std::size_t>(nbytes),
                             static_cast<std::size_t>(BLOCK_SIZE) - off);
            // Original logic: if (chunk < 0) chunk = BLOCK_SIZE - off; This implies chunk could be
            // int. If nbytes is int, chunk should be int. chunk = std::min(nbytes,
            // static_cast<int>(BLOCK_SIZE - off)); if (chunk < 0) chunk =
            // static_cast<int>(BLOCK_SIZE - off); For safety, let's keep chunk as std::size_t as
            // it's a count of bytes. The negative check was likely for an old definition of min or
            // types.

            if (rw_flag == READING) {
                // f_size is int64_t, position is int32_t. bytes_left is int32_t.
                bytes_left = static_cast<int32_t>(f_size - position);
                if (bytes_left <= 0)
                    break;
                else if (chunk > static_cast<std::size_t>(bytes_left)) // chunk is std::size_t
                    chunk = static_cast<std::size_t>(bytes_left);
            }

            /* Read or write 'chunk' bytes. */
            // position is int32_t, off is std::size_t, chunk is std::size_t.
            r = rw_chunk(rip, position, off, chunk, rw_flag, buffer, seg, usr);
            if (r != OK)
                break; /* EOF reached */
            if (rdwt_err < 0)
                break;

            /* Update counters and pointers. */
            buffer += chunk;                   /* user buffer address */
            nbytes -= static_cast<int>(chunk); /* bytes yet to be read (nbytes is int) */
            cum_io += chunk;                   /* bytes read so far (cum_io is std::size_t) */
            position +=
                static_cast<int32_t>(chunk); /* position within the file (position is int32_t) */
            virg = FALSE;                    /* tells pipe_check() that data has been copied */
        }
    }

    /* On write, update file size and access time. */
    if (rw_flag == WRITING) {
        // position is int32_t, f_size is int64_t
        if (mode_word != I_CHAR_SPECIAL && mode_word != I_BLOCK_SPECIAL && position > f_size)
            compat_set_size(
                rip, static_cast<int64_t>(position)); // compat_set_size takes file_pos64 (int64_t)
        rip->i_modtime = clock_time();                // clock_time returns real_time (int64_t)
        rip->i_dirt = DIRTY;
    } else {
        // position is int32_t, compat_get_size returns file_pos64 (int64_t)
        if (rip->i_pipe && static_cast<int64_t>(position) >= compat_get_size(rip)) {
            /* Reset pipe pointers. */
            compat_set_size(rip, 0);                     /* no data left */
            position = 0;                                /* reset reader(s) */
            if ((wf = find_filp(rip, W_BIT)) != nullptr) // NIL_FILP -> nullptr
                wf->filp_pos = 0;                        // filp_pos is file_pos (int32_t)
        }
    }
    f->filp_pos = position; // position is int32_t

    /* Check to see if read-ahead is called for, and if so, set it up. */
    if (rw_flag == READING && rip->i_seek == NO_SEEK && position % BLOCK_SIZE == 0 &&
        (mode_word == I_REGULAR || mode_word == I_DIRECTORY)) {
        rdahed_inode = rip;
        rdahedpos = position; // rdahedpos is file_pos (int32_t)
    }
    if (mode_word == I_REGULAR)
        rip->i_seek = NO_SEEK;

    if (rdwt_err != OK)
        r = rdwt_err;    /* check for disk error */
    if (rdwt_err == EOF) // cum_io is std::size_t, r is int
        r = static_cast<int>(cum_io);
    return (r == OK ? static_cast<int>(cum_io) : r); // cum_io is std::size_t
}

/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
// Modernized signature
static int rw_chunk(struct inode *rip, int32_t position, std::size_t off, std::size_t chunk,
                    int rw_flag, char *buff, int seg, int usr) {
    /* Read or write (part of) a block. */

    register struct buf *bp;
    register int r;
    int dir, n, block_spec;
    uint16_t b;   // block_nr -> uint16_t
    uint16_t dev; // dev_nr -> uint16_t
    extern struct buf *get_block(),
        *new_block(); // Assuming new_block exists or is a typo for something else
    extern uint16_t read_map(struct inode * rip, int32_t position); // Modernized read_map

    block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL; // i_mode is mask_bits (uint16_t)
    if (block_spec) {
        // position is int32_t, BLOCK_SIZE is int. b is uint16_t.
        b = static_cast<uint16_t>(static_cast<uint32_t>(position) / BLOCK_SIZE);
        dev = static_cast<uint16_t>(rip->i_zone[0]); // i_zone[0] is zone_nr (uint16_t)
    } else {
        b = read_map(rip, position); // read_map returns block_nr (uint16_t)
        dev = rip->i_dev;            // i_dev is dev_nr (uint16_t)
    }

    if (!block_spec && b == kNoBlock) { // kNoBlock is block_nr (uint16_t)
        if (rw_flag == READING) {
            /* Reading from a nonexistent block.  Must read as all zeros. */
            bp = get_block(kNoDev, kNoBlock, IoMode::Normal);
            /* get a buffer */ // kNoDev is dev_nr (uint16_t)
            zero_block(bp);
        } else {
            /* Writing to a nonexistent block. Create and enter in inode. */
            if ((bp = new_block(rip, position)) == NIL_BUF) // NIL_BUF is (struct buf*)nullptr
                return (err_code);
        }
    } else {
        /* Normally an existing block to be partially overwritten is first read
         * in.  However, a full block need not be read in.  If it is already in
         * the cache, acquire it, otherwise just acquire a free buffer.
         */
        // chunk is std::size_t, BLOCK_SIZE is int
        n = (rw_flag == WRITING && chunk == static_cast<std::size_t>(BLOCK_SIZE) ? IoMode::NoRead
                                                                                 : IoMode::Normal);
        // position is int32_t, compat_get_size returns file_pos64 (int64_t), off is std::size_t
        if (rw_flag == WRITING && off == 0 &&
            static_cast<int64_t>(position) >= compat_get_size(rip))
            n = IoMode::NoRead;
        bp = get_block(dev, b, n); // dev, b are uint16_t
    }

    /* In all cases, bp now points to a valid buffer. */
    // chunk is std::size_t, BLOCK_SIZE is int, position is int32_t, off is std::size_t
    if (rw_flag == WRITING && chunk != static_cast<std::size_t>(BLOCK_SIZE) && !block_spec &&
        static_cast<int64_t>(position) >= compat_get_size(rip) && off == 0)
        zero_block(bp);
    dir = (rw_flag == READING ? TO_USER : FROM_USER);
    // buff is char*. chunk is std::size_t. bp->b_data is char*. off is std::size_t.
    // rw_user expects (..., std::size_t vir, std::size_t bytes, ...)
    r = rw_user(seg, usr, reinterpret_cast<std::size_t>(buff), chunk, bp->b_data + off, dir);
    if (rw_flag == WRITING)
        bp->b_dirt = DIRTY;
    // off and chunk are std::size_t, BLOCK_SIZE is int
    n = (off + chunk == static_cast<std::size_t>(BLOCK_SIZE) ? BlockType::FullData
                                                             : BlockType::PartialData);
    put_block(bp, n);
    return (r);
}

/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
// Modernized signature
PUBLIC uint16_t read_map(struct inode *rip,
                         int32_t position) { // block_nr -> uint16_t, file_pos -> int32_t
    /* Given an inode and a position within the corresponding file, locate the
     * block (not zone) number in which that position is to be found and return
     * it.
     */

    register struct buf *bp;
    uint16_t z;                      // zone_nr -> uint16_t
    uint16_t b;                      // block_nr -> uint16_t
    int32_t excess, zone, block_pos; // Were long, but derived from file_pos (int32_t)
    int scale, boff;                 // scale_factor returns int. boff is offset in block.
    extern struct buf *get_block();

    scale = scale_factor(rip);          /* for block-zone conversion (returns int) */
    block_pos = position / BLOCK_SIZE;  /* position is int32_t, BLOCK_SIZE is int -> int32_t */
    zone = block_pos >> scale;          /* int32_t >> int -> int32_t */
    boff = block_pos - (zone << scale); /* int32_t */

    /* Is 'position' to be found in the inode itself? */
    if (zone < NR_DZONE_NUM) { // NR_DZONE_NUM is int
        // rip->i_zone is zone_nr[] (uint16_t[]). zone is int32_t, but should be small index.
        if ((z = rip->i_zone[static_cast<std::size_t>(zone)]) ==
            kNoZone)           // kNoZone is zone_nr (uint16_t)
            return (kNoBlock); // kNoBlock is block_nr (uint16_t)
        // z is uint16_t, scale is int, boff is int32_t. b is uint16_t.
        b = static_cast<uint16_t>((static_cast<uint32_t>(z) << scale) + boff);
        return (b);
    }

    /* It is not in the inode, so it must be single or double indirect. */
    excess = zone - NR_DZONE_NUM; /* first NR_DZONE_NUM don't count (excess is int32_t) */

    if (excess < NR_INDIRECTS) { // NR_INDIRECTS is std::size_t (was int)
        /* 'position' can be located via the single indirect block. */
        z = rip->i_zone[NR_DZONE_NUM]; // z is uint16_t
    } else {
        /* 'position' can be located via the double indirect block. */
        if ((z = rip->i_zone[NR_DZONE_NUM + 1]) == kNoZone)
            return (kNoBlock);
        excess -= static_cast<int32_t>(NR_INDIRECTS); /* single indir doesn't count */
        // z is uint16_t, b is uint16_t
        b = static_cast<uint16_t>(static_cast<uint32_t>(z) << scale);
        bp = get_block(rip->i_dev, b, IoMode::Normal);
        /* get double indirect block */ // rip->i_dev is dev_nr (uint16_t)
        // bp->b_ind is zone_nr[] (uint16_t[]). excess / NR_INDIRECTS is int32_t / size_t.
        z = bp->b_ind[static_cast<std::size_t>(excess / static_cast<int32_t>(NR_INDIRECTS))];
        put_block(bp, BlockType::Indirect);                   /* release double ind block */
        excess = excess % static_cast<int32_t>(NR_INDIRECTS); /* index into single ind blk */
    }

    /* 'z' is zone number for single indirect block; 'excess' is index into it. */
    if (z == kNoZone)
        return (kNoBlock);
    // z is uint16_t, b is uint16_t
    b = static_cast<uint16_t>(static_cast<uint32_t>(z) << scale);
    bp = get_block(rip->i_dev, b, IoMode::Normal);   /* get single indirect block */
    z = bp->b_ind[static_cast<std::size_t>(excess)]; // excess is int32_t
    put_block(bp, BlockType::Indirect);              /* release single indirect blk */
    if (z == kNoZone)
        return (kNoBlock);
    // z is uint16_t, boff is int32_t. b is uint16_t.
    b = static_cast<uint16_t>((static_cast<uint32_t>(z) << scale) + boff);
    return (b);
}

/*===========================================================================*
 *				rw_user					     *
 *===========================================================================*/
// Modernized signature
PUBLIC int rw_user(int s, int u, std::size_t vir, std::size_t bytes, char *buff, int direction) {
    // s, u, direction are int. vir, bytes are std::size_t (vir_bytes). buff is char*.
    /* Transfer a block of data.  Two options exist, depending on 'direction':
     *     TO_USER:     Move from FS space to user virtual space
     *     FROM_USER:   Move from user virtual space to FS space
     */

    if (direction == TO_USER) {
        /* Write from FS space to user space. */
        src_space(umess) = D;
        src_proc_nr(umess) = FS_PROC_NR;
        // SRC_BUFFER is a macro for message field (char* type, e.g. m1p1)
        // buff is char*
        src_buffer(umess) = buff;
        dst_space(umess) = s;
        dst_proc_nr(umess) = u;
        // DST_BUFFER is char*. vir is std::size_t (pointer value).
        dst_buffer(umess) = reinterpret_cast<char *>(vir);
    } else {
        /* Read from user space to FS space. */
        src_space(umess) = s;
        src_proc_nr(umess) = u;
        src_buffer(umess) = reinterpret_cast<char *>(vir);
        dst_space(umess) = D;
        dst_proc_nr(umess) = FS_PROC_NR;
        dst_buffer(umess) = buff;
    }

    // COPY_BYTES is a macro for message field (int type, e.g. m1i2)
    // bytes is std::size_t. This is a potential narrowing if bytes > INT_MAX.
    copy_bytes(umess) = static_cast<int>(bytes);
    sys_copy(&umess);
    return (umess.m_type);
}

/*===========================================================================*
 *				read_ahead				     *
 *===========================================================================*/
PUBLIC void read_ahead() { // Modernized signature (was already using ())
    /* Read a block into the cache before it is needed. */

    register struct inode *rip;
    struct buf *bp;
    uint16_t b; // block_nr -> uint16_t
    extern struct buf *get_block();

    rip = rdahed_inode;     /* pointer to inode to read ahead from */
    rdahed_inode = nullptr; /* turn off read ahead (NIL_INODE -> nullptr) */
    // rdahedpos is file_pos (int32_t). read_map takes int32_t, returns block_nr (uint16_t).
    if ((b = read_map(rip, rdahedpos)) == kNoBlock) // kNoBlock is block_nr (uint16_t)
        return;                                     /* at EOF */
    // rip->i_dev is dev_nr (uint16_t). b is uint16_t.
    bp = get_block(rip->i_dev, b, IoMode::Normal);
    put_block(bp, PARTIAL_DATA_BLOCK);
}

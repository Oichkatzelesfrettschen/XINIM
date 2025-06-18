// Modernized for C++23

/* This file contains the procedures for creating, opening, closing, and
 * seeking on files.
 *
 * The entry points into this file are
 *   do_creat:	perform the CREAT system call
 *   do_mknod:	perform the MKNOD system call
 *   do_open:	perform the OPEN system call
 *   do_close:	perform the CLOSE system call
 *   do_lseek:  perform the LSEEK system call
 */

#include "../h/callnr.hpp"
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
#include "type.hpp"
#include <cstddef> // For nullptr
#include <cstdint> // For uint16_t, int32_t, int64_t etc.

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT | W_BIT, 0};

/*===========================================================================*
 *				do_creat				     *
 *===========================================================================*/
int do_creat() {
    /* Perform the creat(name, mode) system call. */

    register struct inode *rip;
    register int r;
    register mask_bits bits;
    struct filp *fil_ptr;
    int file_d;
    extern struct inode *new_node();

    /* See if name ok and file descriptor and filp slots are available. */
    if (fetch_name(name, name_length, M3) != OK) // name, name_length are from param.hpp (message)
        return (err_code);
    // W_BIT is int. get_fd expects mask_bits (uint16_t) for its first arg if it's 'bits_needed'.
    // Assuming get_fd's first param is mode/access bits.
    if ((r = get_fd(static_cast<uint16_t>(W_BIT), &file_d, &fil_ptr)) != OK)
        return (r);

    /* Create a new inode by calling new_node(). */
    // mode is int. I_REGULAR, ALL_MODES are int. fp_umask is mask_bits (uint16_t).
    bits = static_cast<uint16_t>(I_REGULAR | (mode & ALL_MODES & fp->fp_umask));
    rip = new_node(user_path, bits, NO_ZONE); // NO_ZONE is zone_nr (uint16_t)
    r = err_code;
    if (r != OK && r != ErrorCode::EEXIST)
        return (r);

    /* At this point two possibilities exist: the given path did not exist
     * and has been created, or it pre-existed.  In the later case, truncate
     * if possible, otherwise return an error.
     */
    if (r == ErrorCode::EEXIST) {
        /* File exists already. */
        switch (rip->i_mode & I_TYPE) { // i_mode is mask_bits (uint16_t), I_TYPE is int
        case I_REGULAR:                 /* truncate regular file */
            if ((r = forbidden(rip, static_cast<uint16_t>(W_BIT), 0)) ==
                OK) // W_BIT is int, forbidden expects mask_bits
                truncate(rip);
            break;

        case I_DIRECTORY: /* can't truncate directory */
            r = ErrorCode::EISDIR;
            break;

        case I_CHAR_SPECIAL: /* special files are special */
        case I_BLOCK_SPECIAL:
            if ((r = forbidden(rip, static_cast<uint16_t>(W_BIT), 0)) !=
                OK) // W_BIT is int, forbidden expects mask_bits
                break;
            // rip->i_zone[0] is zone_nr (uint16_t). dev_open 1st param is dev_nr (uint16_t).
            // dev_open 2nd param is int access_bits. W_BIT is int.
            r = dev_open(static_cast<uint16_t>(rip->i_zone[0]), W_BIT);
            break;
        }
    }

    /* If error, return inode. */
    if (r != OK) {
        put_inode(rip);
        return (r);
    }

    /* Claim the file descriptor and filp slot and fill them in. */
    fp->fp_filp[file_d] = fil_ptr;
    fil_ptr->filp_count = 1;
    fil_ptr->filp_ino = rip;
    return (file_d);
}

/*===========================================================================*
 *				do_mknod				     *
 *===========================================================================*/
int do_mknod() {
    /* Perform the mknod(name, mode, addr) system call. */

    register mask_bits bits;

    if (!super_user)
        return (ErrorCode::EPERM);                 /* only super_user may make nodes */
    if (fetch_name(name1, name1_length, M1) != OK) // name1, name1_length from param.hpp
        return (err_code);
    // mode is int. I_TYPE, ALL_MODES are int. fp_umask is mask_bits (uint16_t).
    bits = static_cast<uint16_t>((mode & I_TYPE) | (mode & ALL_MODES & fp->fp_umask));
    // addr is int (from m.m1_i3 via param.hpp). new_node expects zone_nr (uint16_t) for 3rd param.
    put_inode(new_node(user_path, bits, static_cast<uint16_t>(addr)));
    return (err_code);
}

/*===========================================================================*
 *				new_node				     *
 *===========================================================================*/
// Changed to modern C++ signature, PRIVATE becomes static
static struct inode *new_node(char *path, uint16_t bits, uint16_t z0) {
    // path is char*
    // bits is mask_bits (uint16_t)
    // z0 is zone_nr (uint16_t)
    /* This function is called by do_creat() and do_mknod().  In both cases it
     * allocates a new inode, makes a directory entry for it on the path 'path',
     * and initializes it.  It returns a pointer to the inode if it can do this;
     * err_code is set to OK or ErrorCode::EEXIST. If it can't, it returns nullptr and
     * 'err_code' contains the appropriate message.
     */

    register struct inode *rlast_dir_ptr, *rip;
    register int r;
    char string[NAME_SIZE]; // NAME_SIZE is std::size_t (was int)
    extern struct inode *alloc_inode(uint16_t,
                                     uint16_t); // Ensure this matches modernized alloc_inode
    extern struct inode *advance(struct inode *, const char *); // Assuming advance signature
    extern struct inode *last_dir(const char *, char *);        // Assuming last_dir signature

    /* See if the path can be opened down to the last directory. */
    if ((rlast_dir_ptr = last_dir(path, string)) == nullptr) // NIL_INODE -> nullptr
        return (nullptr);                                    // NIL_INODE -> nullptr

    /* The final directory is accessible. Get final component of the path. */
    rip = advance(rlast_dir_ptr, string);
    if (rip == nullptr && err_code == ErrorCode::ENOENT) { // NIL_INODE -> nullptr
        /* Last path component does not exist.  Make new directory entry. */
        // rlast_dir_ptr->i_dev is dev_nr (uint16_t). bits is uint16_t.
        if ((rip = alloc_inode(rlast_dir_ptr->i_dev, bits)) == nullptr) { // NIL_INODE -> nullptr
            /* Can't creat new inode: out of inodes. */
            put_inode(rlast_dir_ptr);
            return (nullptr); // NIL_INODE -> nullptr
        }

        /* Force inode to the disk before making directory entry to make
         * the system more robust in the face of a crash: an inode with
         * no directory entry is much better than the opposite.
         */
        rip->i_nlinks++;        // i_nlinks is links (uint8_t)
        rip->i_zone[0] = z0;    // i_zone[0] is zone_nr (uint16_t), z0 is uint16_t
        rw_inode(rip, WRITING); /* force inode to disk now */

        /* New inode acquired.  Try to make directory entry. */
        // rip->i_num is inode_nr (uint16_t). search_dir expects inode_nr* for 3rd param.
        if ((r = search_dir(rlast_dir_ptr, string, &rip->i_num, ENTER)) != OK) {
            put_inode(rlast_dir_ptr);
            rip->i_nlinks--;     /* pity, have to free disk inode */
            rip->i_dirt = DIRTY; /* dirty inodes are written out */
            put_inode(rip);      /* this call frees the inode */
            err_code = r;
            return (nullptr); // NIL_INODE -> nullptr
        }

    } else {
        /* Either last component exists, or there is some problem. */
        if (rip != nullptr) // NIL_INODE -> nullptr
            r = ErrorCode::EEXIST;
        else
            r = err_code;
    }

    /* Return the directory inode and exit. */
    put_inode(rlast_dir_ptr);
    err_code = r;
    return (rip);
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
int do_open() {
    /* Perform the open(name, mode) system call. */

    register struct inode *rip;
    struct filp *fil_ptr;
    register int r;
    register mask_bits bits;
    int file_d;
    extern struct inode *eat_path();

    /* See if file descriptor and filp slots are available.  The variable
     * 'mode' is 0 for read, 1 for write, 2 for read+write.  The variable
     * 'bits' needs to be R_BIT, W_BIT, and R_BIT|W_BIT respectively.
     */
    if (mode < 0 || mode > 2) // mode is int from param.hpp
        return (ErrorCode::EINVAL);
    if (fetch_name(name, name_length, M3) != OK) // name, name_length from param.hpp
        return (err_code);
    bits = static_cast<uint16_t>(
        mode_map[mode]); // mode_map elements are int. bits is mask_bits (uint16_t).
    // get_fd expects mask_bits (uint16_t) for its first arg if it's 'bits_needed'.
    if ((r = get_fd(bits, &file_d, &fil_ptr)) != OK)
        return (r);

    /* Scan path name. */
    if ((rip = eat_path(user_path)) == nullptr) // NIL_INODE -> nullptr
        return (err_code);

    // forbidden expects mask_bits (uint16_t) for its second arg. bits is uint16_t.
    if ((r = forbidden(rip, bits, 0)) != OK) {
        put_inode(rip); /* can't open: protection violation */
        return (r);
    }

    /* Opening regular files, directories and special files are different. */
    switch (rip->i_mode & I_TYPE) {
    case I_DIRECTORY:                              // I_DIRECTORY is int constant
        if (bits & static_cast<uint16_t>(W_BIT)) { // bits is uint16_t, W_BIT is int
            put_inode(rip);
            return (ErrorCode::EISDIR);
        }
        break;

    case I_CHAR_SPECIAL: // I_CHAR_SPECIAL is int constant
        /* Assume that first open of char special file is controlling tty. */
        // fp->fs_tty should be dev_nr (uint16_t). rip->i_zone[0] is zone_nr (uint16_t).
        if (fp->fs_tty == 0) // Assuming 0 is a valid "no tty" value for dev_nr.
            fp->fs_tty = static_cast<uint16_t>(rip->i_zone[0]);
        // dev_open 1st param is dev_nr (uint16_t), 2nd is int. bits is uint16_t.
        dev_open(static_cast<uint16_t>(rip->i_zone[0]), static_cast<int>(bits));
        break;

    case I_BLOCK_SPECIAL: // I_BLOCK_SPECIAL is int constant
        dev_open(static_cast<uint16_t>(rip->i_zone[0]), static_cast<int>(bits));
        break;
    }

    /* Claim the file descriptor and filp slot and fill them in. */
    fp->fp_filp[file_d] = fil_ptr;
    fil_ptr->filp_count = 1;
    fil_ptr->filp_ino = rip;
    return (file_d);
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
int do_close() {
    /* Perform the close(fd) system call. */

    register struct filp *rfilp;
    register struct inode *rip;
    int rw;
    int mode_word;
    extern struct filp *get_filp();

    /* First locate the inode that belongs to the file descriptor. */
    if ((rfilp = get_filp(fd)) == nullptr) // NIL_FILP -> nullptr
        return (err_code);
    rip = rfilp->filp_ino; /* 'rip' points to the inode */

    /* Check to see if the file is special. */
    mode_word = rip->i_mode & I_TYPE;
    if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
        if (mode_word == I_BLOCK_SPECIAL) {
            /* Invalidate cache entries unless special is mounted or ROOT.*/
            do_sync(); /* purge cache */
            // mounted takes struct inode*. FALSE is likely 0.
            if (mounted(rip) == static_cast<int>(FALSE)) // Assuming FALSE is int 0
                invalidate(
                    static_cast<uint16_t>(rip->i_zone[0])); // invalidate expects dev_nr (uint16_t)
        }
        dev_close(static_cast<uint16_t>(rip->i_zone[0])); // dev_close expects dev_nr (uint16_t)
    }

    /* If the inode being closed is a pipe, release everyone hanging on it. */
    if (rfilp->filp_ino->i_pipe) {
        rw = (rfilp->filp_mode & R_BIT ? WRITE : READ);
        release(rfilp->filp_ino, rw, NR_PROCS);
    }

    /* If a write has been done, the inode is already marked as DIRTY. */
    if (--rfilp->filp_count == 0)
        put_inode(rfilp->filp_ino);

    fp->fp_filp[fd] = nullptr; // NIL_FILP -> nullptr
    return (OK);
}

/*===========================================================================*
 *				do_lseek				     *
 *===========================================================================*/
int do_lseek() {
    /* Perform the lseek(ls_fd, offset, whence) system call. */

    register struct filp *rfilp;
    register file_pos64 pos;
    extern struct filp *get_filp();

    /* Check to see if the file descriptor is valid. */
    if ((rfilp = get_filp(ls_fd)) == nullptr) // NIL_FILP -> nullptr
        return (err_code);

    /* No lseek on pipes. */
    if (rfilp->filp_ino->i_pipe == I_PIPE)
        return (ErrorCode::ESPIPE);

    /* The value of 'whence' determines the algorithm to use. */
    // whence is int from param.hpp. offset is int64_t from param.hpp.
    switch (whence) {
    case 0:           // SEEK_SET
        pos = offset; // int64_t = int64_t
        break;
    case 1: // SEEK_CUR
        // rfilp->filp_pos is file_pos (int32_t). offset is int64_t.
        pos = static_cast<int64_t>(rfilp->filp_pos) + offset;
        break;
    case 2: // SEEK_END
        // compat_get_size returns file_pos64 (int64_t). offset is int64_t.
        pos = compat_get_size(rfilp->filp_ino) + offset;
        break;
    default:
        return (ErrorCode::EINVAL);
    }
    if (pos < 0) // pos is int64_t
        return (ErrorCode::EINVAL);

    rfilp->filp_ino->i_seek = ISEEK; /* inhibit read ahead */
    // rfilp->filp_pos is file_pos (int32_t). pos is int64_t. This is a narrowing conversion.
    // This was identified as a potential issue. Explicit cast highlights it.
    rfilp->filp_pos = static_cast<int32_t>(pos);

    // reply_l1 is a macro for m1.m2_l1() which is int64_t (after message struct modernization)
    // pos is int64_t.
    reply_l1 = pos;
    return (OK);
}

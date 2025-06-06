/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* This file contains the procedures that manipulate file descriptors.
 *
 * The entry points into this file are
 *   get_fd:	look for free file descriptor and free filp slots
 *   get_filp:	look up the filp entry for a given file descriptor
 *   find_filp:	find a filp slot that points to a given inode
 */

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "type.hpp"

/*===========================================================================*
 *				get_fd					     *
 *===========================================================================*/
PUBLIC int get_fd(bits, k, fpt)
mask_bits bits;    /* mode of the file to be created (RWX bits) */
int *k;            /* place to return file descriptor */
struct filp **fpt; /* place to return filp slot */
{
    /* Look for a free file descriptor and a free filp slot.  Fill in the mode word
     * in the latter, but don't claim either one yet, since the open() or creat()
     * may yet fail.
     */

    register struct filp *f;
    register int i;

    *k = -1; /* we need a way to tell if file desc found */

    /* Search the fproc table for a free file descriptor. */
    for (i = 0; i < NR_FDS; i++) {
        if (fp->fp_filp[i] == NIL_FILP) {
            /* A file descriptor has been located. */
            *k = i;
            break;
        }
    }

    /* Check to see if a file descriptor has been found. */
    if (*k < 0)
        return (ErrorCode::EMFILE); /* this is why we initialized k to -1 */

    /* Now that a file descriptor has been found, look for a free filp slot. */
    for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
        if (f->filp_count == 0) {
            f->filp_mode = bits;
            f->filp_pos = 0L;
            *fpt = f;
            return (OK);
        }
    }

    /* If control passes here, the filp table must be full.  Report that back. */
    return (ErrorCode::ENFILE);
}

/*===========================================================================*
 *				get_filp				     *
 *===========================================================================*/
PUBLIC struct filp *get_filp(fild)
int fild; /* file descriptor */
{
    /* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */

    err_code = ErrorCode::EBADF;
    if (fild < 0 || fild >= NR_FDS)
        return (NIL_FILP);
    return (fp->fp_filp[fild]); /* may also be NIL_FILP */
}

/*===========================================================================*
 *				find_filp				     *
 *===========================================================================*/
PUBLIC struct filp *find_filp(rip, bits)
register struct inode *rip; /* inode referred to by the filp to be found */
int bits;                   /* mode of the filp to be found (RWX bits) */
{
    /* Find a filp slot that refers to the inode 'rip' in a way as described
     * by the mode bit 'bits'. Used for determining whether somebody is still
     * interested in either end of a pipe; other applications are conceivable.
     * Like 'get_fd' it performs its job by linear search through the filp table.
     */

    register struct filp *f;

    for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
        if (f->filp_count != 0 && f->filp_ino == rip && (f->filp_mode & bits)) {
            return (f);
        }
    }

    /* If control passes here, the filp wasn't there.  Report that back. */
    return (NIL_FILP);
}

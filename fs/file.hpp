/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
    mask_bits filp_mode;    /* RW bits, telling how file is opened */
    int filp_count;         /* how many file descriptors share this slot? */
    struct inode *filp_ino; /* pointer to the inode */
    file_pos filp_pos;      /* file position */
} filp[NR_FILPS];

#define NIL_FILP (struct filp *)0 /* indicates absence of a filp slot */

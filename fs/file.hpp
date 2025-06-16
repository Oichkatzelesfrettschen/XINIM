#pragma once
// Modernized for C++23

/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
#include "../h/type.hpp" // For mask_bits, file_pos
#include <cstddef>       // For nullptr

    mask_bits filp_mode;    /* RW bits, telling how file is opened */
    int filp_count;         /* how many file descriptors share this slot? */
    struct inode *filp_ino; /* pointer to the inode */
    file_pos filp_pos;      /* file position (file_pos is int32_t) */
} filp[NR_FILPS];

// #define NIL_FILP (struct filp *)0 /* indicates absence of a filp slot */ // Replaced by constexpr
inline constexpr struct filp *NIL_FILP = nullptr;

/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Translation helpers for compatibility with the original Minix FS.
 * These helpers store 64-bit file sizes and extent tables while
 * maintaining the legacy 32-bit structures.
 */

#include "compat.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp"
#include "extent.hpp"
#include "inode.hpp"

/*===========================================================================*
 *                              compat_get_size                              *
 *===========================================================================*/
/* Return the 64-bit size of an inode.
 * ip: inode to query.
 */
PUBLIC file_pos64 compat_get_size(const struct inode *ip) {
    return ip->i_size64 ? ip->i_size64 : (file_pos64)ip->i_size;
}

/*===========================================================================*
 *                              compat_set_size                              *
 *===========================================================================*/
/* Update both the 64-bit and 32-bit size fields.
 * ip: inode to update.
 * sz: new file size.
 */
PUBLIC void compat_set_size(struct inode *ip, file_pos64 sz) {
    ip->i_size64 = sz;
    ip->i_size = (file_pos)sz;
}

/*===========================================================================*
 *                              init_extended_inode                           *
 *===========================================================================*/
/* Initialize the 64-bit fields of an inode.
 * ip: inode to set up.
 */
PUBLIC void init_extended_inode(struct inode *ip) {
    ip->i_size64 = ip->i_size;
    ip->i_extents = NIL_PTR;
    ip->i_extent_count = 0;
}

/*===========================================================================*
 *                              alloc_extent_table                           *
 *===========================================================================*/
/* Allocate and zero 'count' extents for an inode.
 * ip: inode receiving the extent table.
 * count: number of extents to allocate.
 */
PUBLIC int alloc_extent_table(struct inode *ip, unsigned short count) {

    extent *table;    /* Pointer to newly allocated extent list. */
    unsigned short i; /* Loop counter for initialization.       */

    /* Reject zero-sized tables to avoid undefined behaviour. */
    if (count == 0) {
        ip->i_extents = NIL_PTR;
        ip->i_extent_count = 0;
        return EINVAL;
    }

    /* Allocate memory for the extent table. */
    table = (extent *)safe_malloc((unsigned)(count * sizeof(extent)));
    if (table == NIL_EXTENT) {
        /* Allocation failed.  Ensure inode fields remain clear. */
        ip->i_extents = NIL_PTR;
        ip->i_extent_count = 0;
        return ENOMEM;
    }

    /* Initialize all table entries so the caller starts with empty extents. */
    for (i = 0; i < count; i++) {
        table[i].e_start = NO_ZONE;
        table[i].e_count = 0;
    }

    /* Attach the table to the inode. */
    ip->i_extents = table;
    ip->i_extent_count = count;

    return OK;
}

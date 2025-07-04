/* Translation helpers for compatibility with the original Minix FS.
 * These helpers store 64-bit file sizes and extent tables while
 * maintaining the legacy 32-bit structures.
 */

#include "compat.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp" // C++23 header
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

    // RAII wrapper for the extent table memory.
    SafeBuffer<extent> table_buf(count); // managed table memory
    unsigned short i;                    /* Loop counter for initialization.       */

    /* Reject zero-sized tables to avoid undefined behaviour. */
    if (count == 0) {
        ip->i_extents = NIL_PTR;
        ip->i_extent_count = 0;
        return ErrorCode::EINVAL;
    }

    /* Memory was successfully allocated by SafeBuffer. */

    /* Initialize all table entries so the caller starts with empty extents. */
    for (i = 0; i < count; i++) {
        table_buf[i].e_start = NO_ZONE;
        table_buf[i].e_count = 0;
    }

    /* Attach the table to the inode. */
    ip->i_extents = table_buf.release();
    ip->i_extent_count = count;

    return OK;
}

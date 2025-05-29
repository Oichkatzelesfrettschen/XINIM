/* Translation helpers for compatibility with the original Minix FS.
 * These helpers store 64-bit file sizes and extent tables while
 * maintaining the legacy 32-bit structures.
 */

#include "compat.h"
#include "../h/const.h"
#include "../h/type.h"
#include "extent.h"
#include "inode.h"

/* Initialize extended inode fields. */
PUBLIC void init_extended_inode(struct inode *ip) {
  ip->i_size64 = ip->i_size;
  ip->i_extents = NIL_PTR;
  ip->i_extent_count = 0;
}

/* Allocate extent table for an inode. Placeholder for real allocator. */
PUBLIC int alloc_extent_table(struct inode *ip, unsigned short count) {
  /* Allocation is not implemented; ensure fields reflect failure. */
  ip->i_extents = NIL_PTR;
  ip->i_extent_count = 0;
  (void)count;
  return ENOSYS;
}

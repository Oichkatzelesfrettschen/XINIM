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

/* Allocate and zero an extent table for an inode. */
PUBLIC int alloc_extent_table(struct inode *ip, unsigned short count) {
  /* Function obtains storage for 'count' extents and attaches the table
   * to 'ip'.  It returns OK on success or ENOMEM if the allocation fails.
   */

  extern char *malloc(); /* Memory allocator provided by lib. */
  extent *table;         /* Pointer to newly allocated extent list. */
  unsigned short i;      /* Loop counter for initialization.       */

  /* Reject zero-sized tables to avoid undefined behaviour. */
  if (count == 0) {
    ip->i_extents = NIL_PTR;
    ip->i_extent_count = 0;
    return EINVAL;
  }

  /* Allocate memory for the extent table. */
  table = (extent *)malloc((unsigned)(count * sizeof(extent)));
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

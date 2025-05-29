/* Translation helpers for compatibility with the original Minix FS.
 * These helpers store 64-bit file sizes and extent tables while
 * maintaining the legacy 32-bit structures.
 */

#include "compat.h"
#include "../h/const.h"
#include "../h/type.h"
#include "extent.h"
#include "inode.h"

/*===========================================================================*
 *                              init_extended_inode                           *
 *===========================================================================*/
/* Initialize the 64-bit fields of an inode.
 * ip: inode to set up.
 */
PUBLIC void init_extended_inode(struct inode *ip)
{
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
PUBLIC int alloc_extent_table(struct inode *ip, unsigned short count)
{

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

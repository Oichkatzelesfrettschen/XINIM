/* Compatibility layer for legacy Minix file system structures. */
#ifndef FS_COMPAT_H
#define FS_COMPAT_H

#include "extent.hpp"
#include "inode.hpp"

/* Retrieve the 64-bit size of an inode. */
PUBLIC file_pos64 compat_get_size(const struct inode *ip);

/* Update both 64-bit and 32-bit size fields. */
PUBLIC void compat_set_size(struct inode *ip, file_pos64 sz);

/* Initialize extended fields of an inode. */
PUBLIC void init_extended_inode(struct inode *ip);

/* Allocate an extent table. */
PUBLIC int alloc_extent_table(struct inode *ip, unsigned short count);

#endif /* FS_COMPAT_H */

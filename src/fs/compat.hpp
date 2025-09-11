/**
 * @brief Compatibility layer for legacy Minix file system structures.
 */
#ifndef FS_COMPAT_H
#define FS_COMPAT_H

#include "extent.hpp"
#include "inode.hpp"

/**
 * @brief Retrieve the 64-bit size of an inode.
 * @param ip inode to query.
 * @return 64-bit file size.
 */
file_pos64 compat_get_size(const struct inode *ip);

/**
 * @brief Update both 64-bit and 32-bit size fields.
 * @param ip inode to update.
 * @param sz new file size.
 */
void compat_set_size(struct inode *ip, file_pos64 sz);

/**
 * @brief Initialize the extended fields of an inode.
 * @param ip inode to set up.
 */
void init_extended_inode(struct inode *ip);

/**
 * @brief Allocate an extent table for an inode.
 * @param ip inode receiving the extent table.
 * @param count number of extents to allocate.
 * @return OK on success or ErrorCode on failure.
 */
int alloc_extent_table(struct inode *ip, unsigned short count);

#endif /* FS_COMPAT_H */

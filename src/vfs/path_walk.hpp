#ifndef XINIM_VFS_PATH_WALK_HPP
#define XINIM_VFS_PATH_WALK_HPP

#include "bare_vfs.hpp"

// Walk an absolute path and return the inode of the final component.
// Returns 0 if any component is not found or the path is invalid.
uint32_t path_walk(const char* path);

// Reset the bounded positive/negative path cache.
void path_cache_reset();

// Invalidate one cached child lookup under parent_ino.
void path_cache_invalidate(uint32_t parent_ino,
                           const char* name,
                           uint8_t namelen);

// Walk to the parent directory of path, set *out_name/*out_namelen to the
// final component, return parent inode. Returns 0 on error.
uint32_t path_walk_parent(const char* path,
                          const char** out_name,
                          uint8_t*     out_namelen);

#endif /* XINIM_VFS_PATH_WALK_HPP */

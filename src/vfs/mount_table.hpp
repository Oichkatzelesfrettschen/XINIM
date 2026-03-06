#ifndef XINIM_VFS_MOUNT_TABLE_HPP
#define XINIM_VFS_MOUNT_TABLE_HPP

#include "bare_vfs.hpp"

void              mount_table_init();
int               mount_add(const char* path, uint32_t root_ino, const FsOps* ops);
int               mount_remove(const char* path);
const MountEntry* mount_resolve(const char* path);

#endif /* XINIM_VFS_MOUNT_TABLE_HPP */

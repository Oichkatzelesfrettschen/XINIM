#ifndef XINIM_VFS_DIRENT_HPP
#define XINIM_VFS_DIRENT_HPP

#include "bare_vfs.hpp"

void     dirent_table_init();
uint32_t dirent_block_alloc();
int      dirent_add(uint32_t parent_ino, uint32_t child_ino,
                    uint8_t type, const char* name, uint8_t namelen);
uint32_t dirent_lookup(uint32_t parent_ino, const char* name, uint8_t namelen);
int      dirent_remove(uint32_t parent_ino, const char* name, uint8_t namelen);
int      dirent_readdir(uint32_t parent_ino, DirEntry* buf, int max_entries);

#endif /* XINIM_VFS_DIRENT_HPP */

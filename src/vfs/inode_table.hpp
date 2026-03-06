#ifndef XINIM_VFS_INODE_TABLE_HPP
#define XINIM_VFS_INODE_TABLE_HPP

#include "bare_vfs.hpp"

// Freestanding -- no STL, no heap

void     inode_table_init();
uint32_t inode_alloc();
void     inode_free(uint32_t ino);
RawInode* inode_get(uint32_t ino);

// Data arena
uint32_t data_arena_alloc(uint32_t len);
void     data_arena_free(uint32_t off, uint32_t len);
uint8_t* data_arena_ptr(uint32_t off);

// Exposed for buffer_cache.cpp
extern uint8_t g_data_arena[];

#endif /* XINIM_VFS_INODE_TABLE_HPP */

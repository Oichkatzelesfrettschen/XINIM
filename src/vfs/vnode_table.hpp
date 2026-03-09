#ifndef XINIM_VFS_VNODE_TABLE_HPP
#define XINIM_VFS_VNODE_TABLE_HPP

#include "bare_vfs.hpp"

void vnode_table_init();
VnodeHandle* vnode_acquire(uint32_t ino);
VnodeHandle* vnode_lookup(uint32_t ino);
void vnode_release(uint32_t ino);
void vnode_forget(uint32_t ino);

#endif /* XINIM_VFS_VNODE_TABLE_HPP */

#ifndef XINIM_VFS_FD_TABLE_HPP
#define XINIM_VFS_FD_TABLE_HPP

#include "bare_vfs.hpp"

// VFS-server global FD table (freestanding, no STL)
// Distinct from src/kernel/fd_table.hpp (per-process, hosted runtime)

void     fd_table_init();
int      fd_allocate(uint32_t ino, uint32_t flags);
FdEntry* fd_get(int fd);
int      fd_release(int fd);

#endif /* XINIM_VFS_FD_TABLE_HPP */

#ifndef XINIM_VFS_RAMFS_OPS_HPP
#define XINIM_VFS_RAMFS_OPS_HPP

#include "bare_vfs.hpp"

// Exported ramfs FsOps table.
// Declare with extern "C" to match the definition in ramfs_ops.cpp.
extern "C" const FsOps ramfs_ops;

#endif /* XINIM_VFS_RAMFS_OPS_HPP */

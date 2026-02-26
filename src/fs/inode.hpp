/**
 * @file inode.hpp
 * @brief In-memory inode table definitions.
 *
 * The inode table holds inodes that are currently in use.  In some
 * cases they have been opened by an open() or creat() system call, in other
 * cases the file system itself needs the inode for one reason or another,
 * such as to search a directory for a path name.
 * The first part of the struct holds fields that are present on the
 * disk; the second part holds fields not present on the disk.
 * The disk inode part is also declared in "type.hpp" as 'd_inode'.
 */

#pragma once

#include "const.hpp"
#include <xinim/fs/extent.hpp>

/**
 * @struct inode
 * @brief In-memory inode structure.
 */
EXTERN struct inode {
    mask_bits i_mode;             ///< File type, protection, etc.
    uid i_uid;                    ///< User ID of the file's owner.
    file_pos i_size;              ///< Current file size in bytes.
    file_pos64 i_size64;          ///< 64-bit file size.
    extent *i_extents;            ///< Extent table for extent-based files.
    uint16_t i_extent_count;      ///< Number of extents.
    real_time i_modtime;          ///< Time of last data change.
    gid i_gid;                    ///< Group number.
    links i_nlinks;               ///< Link count.
    zone_nr i_zone[NR_ZONE_NUMS]; ///< Direct/indirect zone numbers.

    /* The following items are not present on the disk. */
    dev_nr i_dev;    ///< Device containing this inode.
    inode_nr i_num;  ///< Inode number on its device.
    int16_t i_count; ///< # times inode used; 0 means slot is free.
    char i_dirt;     ///< CLEAN or DIRTY.
    char i_pipe;     ///< I_PIPE if pipe.
    char i_mount;    ///< I_MOUNT if file mounted on.
    char i_seek;     ///< ISEEK if last op was SEEK.
} inode[NR_INODES];

// #define NIL_INODE (struct inode *)0 /* indicates absence of inode slot */ // Replaced by
// constexpr
inline constexpr struct inode *NIL_INODE = nullptr;

/* Field values.  Note that CLEAN and DIRTY are defined in "const.hpp" */
#define NO_PIPE 0  /* i_pipe is NO_PIPE if inode is not a pipe */
#define I_PIPE 1   /* i_pipe is I_PIPE if inode is a pipe */
#define NO_MOUNT 0 /* i_mount is NO_MOUNT if file not mounted on */
#define I_MOUNT 1  /* i_mount is I_MOUNT if file mounted on */
#define NO_SEEK 0  /* i_seek = NO_SEEK if last op was not SEEK */
#define ISEEK 1    /* i_seek = ISEEK if last op was SEEK */

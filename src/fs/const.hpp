#pragma once
// Modernized for C++23

#include "../"sys/type.hpp" // For uid, gid, bit_nr, block_nr, inode_nr, zone_nr
#include <cstddef>          // For std::size_t
// Forward declarations for structs used in sizeof expressions
struct dir_struct;
struct d_inode;
struct super_block;

/* Tables sizes */
inline constexpr int NR_ZONE_NUMS = 9; /* # zone numbers in an inode */
inline constexpr int NR_BUFS = 20;     /* # blocks in the buffer cache */
// #define NR_BUF_HASH 32                /* size of buf hash table; MUST BE POWER OF 2*/ // Replaced
// by kNrBufHash
inline constexpr int kNrBufHash = 32;              // C++23 constant
inline constexpr int NR_FDS = 20;                  /* max file descriptors per process */
inline constexpr int NR_FILPS = 64;                /* # slots in filp table */
inline constexpr int I_MAP_SLOTS = 4;              /* max # of blocks in the inode bit map */
inline constexpr int ZMAP_SLOTS = 6;               /* max # of blocks in the zone bit map */
inline constexpr int NR_INODES = 32;               /* # slots in "in core" inode table */
inline constexpr int NR_SUPERS = 5;                /* # slots in super block table */
inline constexpr std::size_t NAME_SIZE = 14;       /* # bytes in a directory component */
inline constexpr std::size_t FS_STACK_BYTES = 512; /* size of file system stack */

/* Miscellaneous constants */
inline constexpr int SUPER_MAGIC = 0x137F;          /* magic number contained in super-block */
inline constexpr uid SU_UID = static_cast<uid>(0);  /* super_user's uid */
inline constexpr uid SYS_UID = static_cast<uid>(0); /* uid for processes MM and INIT */
inline constexpr gid SYS_GID = static_cast<gid>(0); /* gid for processes MM and INIT */
inline constexpr int NORMAL = 0;                    /* forces get_block to do disk read */
inline constexpr int NO_READ = 1;                   /* prevents get_block from doing disk read */

inline constexpr int XPIPE = 0; /* used in fp_task when suspended on pipe */
inline constexpr bit_nr NO_BIT =
    static_cast<bit_nr>(0);           /* returned by alloc_bit() to signal failure */
inline constexpr int DUP_MASK = 0100; /* mask to distinguish dup2 from dup */

inline constexpr int LOOK_UP = 0; /* tells search_dir to lookup string */
inline constexpr int ENTER = 1;   /* tells search_dir to make dir entry */
inline constexpr int DELETE = 2;  /* tells search_dir to delete entry */

inline constexpr int CLEAN = 0; /* disk and memory copies identical */
inline constexpr int DIRTY = 1; /* disk and memory copies differ */

inline constexpr block_nr BOOT_BLOCK = static_cast<block_nr>(0);  /* block number of boot block */
inline constexpr block_nr SUPER_BLOCK = static_cast<block_nr>(1); /* block number of super block */
inline constexpr inode_nr ROOT_INODE =
    static_cast<inode_nr>(1); /* inode number for root directory */

/* Derived sizes */
#include "../"sys/const.hpp" // For BLOCK_SIZE
#include "./super.hpp"       // For super_block (needed for sizeof)
#include "./type.hpp"        // For dir_struct, d_inode (needed for sizeof)

inline constexpr std::size_t ZONE_NUM_SIZE = sizeof(zone_nr);              /* # bytes in zone nr*/
inline constexpr int NR_DZONE_NUM = NR_ZONE_NUMS - 2;                      /* # zones in inode */
inline constexpr std::size_t DIR_ENTRY_SIZE = sizeof(dir_struct);          /* # bytes/dir entry */
inline constexpr std::size_t INODE_SIZE = sizeof(d_inode);                 /* bytes in disk inode*/
inline constexpr std::size_t INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;   /* # inodes/disk blk */
inline constexpr std::size_t NR_DIR_ENTRIES = BLOCK_SIZE / DIR_ENTRY_SIZE; /* # dir entries/blk*/
inline constexpr std::size_t NR_INDIRECTS = BLOCK_SIZE / ZONE_NUM_SIZE;    /* # zones/indir blk */
inline constexpr std::size_t INTS_PER_BLOCK = BLOCK_SIZE / sizeof(int);    /* # integers/blk */
inline constexpr std::size_t SUPER_SIZE = sizeof(super_block);             /* super_block size */
inline constexpr std::size_t PIPE_SIZE =
    static_cast<std::size_t>(NR_DZONE_NUM) * BLOCK_SIZE; /* pipe size in bytes*/
inline constexpr std::size_t MAX_ZONES = NR_DZONE_NUM + NR_INDIRECTS + NR_INDIRECTS * NR_INDIRECTS;
/* max # of zones in a file */
#ifndef __cplusplus
#define printf printk
#endif

/* Inode table.  This table holds inodes that are currently in use.  In some
 * cases they have been opened by an open() or creat() system call, in other
 * cases the file system itself needs the inode for one reason or another,
 * such as to search a directory for a path name.
 * The first part of the struct holds fields that are present on the
 * disk; the second part holds fields not present on the disk.
 * The disk inode part is also declared in "type.hpp" as 'd_inode'.
 */

#include "sys/type.hpp" // Explicitly include for types like mask_bits, uid, etc.
#include "extent.hpp"    // Includes "sys/type.hpp" which includes <cstdint>, <cstddef>

EXTERN struct inode {
    mask_bits i_mode;             /* file type, protection, etc. (mask_bits is uint16_t) */
    uid i_uid;                    /* user id of the file's owner (uid is uint16_t) */
    file_pos i_size;              /* current file size in bytes (file_pos is int32_t) */
    file_pos64 i_size64;          /* 64-bit file size (file_pos64 is int64_t) */
    extent *i_extents;            /* extent table for extent-based files */
    uint16_t i_extent_count;      /* number of extents (was unsigned short) */
    real_time i_modtime;          /* when was file data last changed (real_time is int64_t) */
    gid i_gid;                    /* group number (gid is uint8_t) */
    links i_nlinks;               /* how many links to this file (links is uint8_t) */
    zone_nr i_zone[NR_ZONE_NUMS]; /* zone numbers for direct, ind, and dbl ind */

    /* The following items are not present on the disk. */
    dev_nr i_dev;    /* which device is the inode on (dev_nr is uint16_t) */
    inode_nr i_num;  /* inode number on its (minor) device (inode_nr is uint16_t) */
    int16_t i_count; /* # times inode used; 0 means slot is free (was short int) */
    char i_dirt;     /* CLEAN or DIRTY */
    char i_pipe;     /* set to I_PIPE if pipe */
    char i_mount;    /* this bit is set if file mounted on */
    char i_seek;     /* set on LSEEK, cleared on READ/WRITE */
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

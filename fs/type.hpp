/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* Type definitions local to the File System. */

typedef struct {            /* directory entry */
    inode_nr d_inum;        /* inode number */
    char d_name[NAME_SIZE]; /* character string */
} dir_struct;

/* Declaration of the disk inode used in rw_inode(). */
typedef struct {                  /* disk inode.  Memory inode is in "inotab.h" */
    mask_bits i_mode;             /* file type, protection, etc. */
    uid i_uid;                    /* user id of the file's owner */
    file_pos i_size;              /* current file size in bytes */
    file_pos64 i_size64;          /* 64-bit file size */
    real_time i_modtime;          /* when was file data last changed */
    gid i_gid;                    /* group number */
    links i_nlinks;               /* how many links to this file */
    zone_nr i_zone[NR_ZONE_NUMS]; /* block nums for direct, ind, and dbl ind */
} d_inode;

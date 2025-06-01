#pragma once

// Shared definition of the `stat` structure used by both the
// kernel headers in `h/` and the user space headers in `include/`.
// This matches the historical layout from MINIX.

struct stat {
    short int st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short int st_nlink;
    short int st_uid;
    short int st_gid;
    short int st_rdev;
    long st_size;
    long st_atime;
    long st_mtime;
    long st_ctime;
};

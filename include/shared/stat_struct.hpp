#pragma once

// Shared definition of the `stat` structure used by both the
// kernel headers in `h/` and the user space headers in `include/`.
// This matches the historical layout from MINIX.
#include <xinim/core_types.hpp> // For xinim::dev_t, xinim::ino_t etc.

struct stat {
    xinim::dev_t st_dev;     // Was short int
    xinim::ino_t st_ino;     // Was unsigned short
    xinim::mode_t st_mode;   // Was unsigned short
    xinim::nlink_t st_nlink; // Was short int
    xinim::uid_t st_uid;     // Was short int
    xinim::gid_t st_gid;     // Was short int
    xinim::dev_t st_rdev;    // Was short int (assuming device number)
    xinim::off_t st_size;    // Was long (xinim::off_t is std::int64_t)
    xinim::time_t st_atime;  // Was long (xinim::time_t is std::int64_t)
    xinim::time_t st_mtime;  // Was long
    xinim::time_t st_ctime;  // Was long
};

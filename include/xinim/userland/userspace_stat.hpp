#pragma once

#include <stdint.h>

namespace xinim::userland {

struct UserspaceStat {
    uint32_t st_dev;
    uint32_t st_ino;
    uint16_t st_mode;
    uint16_t st_nlink;
    uint16_t st_uid;
    uint16_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    int32_t st_atime;
    uint32_t st_atime_nsec;
    int32_t st_mtime;
    uint32_t st_mtime_nsec;
    int32_t st_ctime;
    uint32_t st_ctime_nsec;
    uint32_t st_unused4;
    uint32_t st_unused5;
};

inline constexpr uint16_t kStatTypeMask = 0xF000U;
inline constexpr uint16_t kStatDirectory = 0x4000U;
inline constexpr uint16_t kStatRegular = 0x8000U;

} // namespace xinim::userland

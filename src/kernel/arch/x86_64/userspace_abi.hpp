#pragma once

#include "../../i486/bootfs.hpp"

#include <cstddef>
#include <cstdint>

namespace xinim::kernel::x86_64 {

    struct UserspaceTimeValue64 {
        int64_t seconds;
        int64_t microseconds;
    };

    struct UserspaceTimeSpec64 {
        int64_t seconds;
        int64_t nanoseconds;
    };

    struct UserspaceResourceLimit64 {
        uint64_t current;
        uint64_t maximum;
    };

    struct UserspaceResourceUsage64 {
        UserspaceTimeValue64 user_time;
        UserspaceTimeValue64 system_time;
        int64_t counters[14];
    };

    static_assert(sizeof(UserspaceTimeValue64) == 16U);
    static_assert(sizeof(UserspaceTimeSpec64) == 16U);
    static_assert(sizeof(UserspaceResourceLimit64) == 16U);
    static_assert(sizeof(UserspaceResourceUsage64) == 144U);
    static_assert(offsetof(UserspaceResourceUsage64, user_time) == 0U);
    static_assert(offsetof(UserspaceResourceUsage64, system_time) == 16U);
    static_assert(offsetof(UserspaceResourceUsage64, counters) == 32U);

    struct UserspaceStat64 {
        uint64_t device;
        uint64_t inode;
        uint64_t link_count;
        uint32_t mode;
        uint32_t user_id;
        uint32_t group_id;
        uint32_t padding;
        uint64_t special_device;
        int64_t size;
        int64_t block_size;
        int64_t block_count;
        int64_t access_time;
        uint64_t access_time_nanoseconds;
        int64_t modification_time;
        uint64_t modification_time_nanoseconds;
        int64_t status_change_time;
        uint64_t status_change_time_nanoseconds;
        int64_t unused[3];
    };

    static_assert(sizeof(UserspaceStat64) == 144U);
    static_assert(offsetof(UserspaceStat64, mode) == 24U);
    static_assert(offsetof(UserspaceStat64, special_device) == 40U);
    static_assert(offsetof(UserspaceStat64, size) == 48U);
    static_assert(offsetof(UserspaceStat64, access_time) == 72U);
    static_assert(offsetof(UserspaceStat64, unused) == 120U);

    [[nodiscard]] UserspaceStat64 serialize_stat64(const bootfs::FileStatus &source) noexcept;

    [[nodiscard]] size_t directory_record_size(size_t name_length, bool use_dirent64) noexcept;

    [[nodiscard]] bool serialize_directory_record(uint8_t *output, size_t capacity,
                                                  const char *name, uint64_t inode,
                                                  int64_t next_offset, uint8_t type,
                                                  bool use_dirent64, size_t &record_size) noexcept;

} // namespace xinim::kernel::x86_64

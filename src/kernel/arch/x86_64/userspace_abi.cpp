#include "userspace_abi.hpp"

#include <cstring>
#include <limits>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr size_t kLegacyNameOffset = 18U;
        constexpr size_t kDirent64TypeOffset = 18U;
        constexpr size_t kDirent64NameOffset = 19U;
        constexpr size_t kDirectoryRecordAlignment = 8U;

        [[nodiscard]] size_t string_length(const char *text) noexcept {
            size_t length = 0U;
            if (text == nullptr) {
                return 0U;
            }
            while (text[length] != '\0') {
                ++length;
            }
            return length;
        }

    } // namespace

    UserspaceStat64 serialize_stat64(const bootfs::FileStatus &source) noexcept {
        UserspaceStat64 destination{};
        destination.device = source.device;
        destination.inode = source.inode;
        destination.link_count = source.link_count;
        destination.mode = source.mode;
        destination.user_id = source.user_id;
        destination.group_id = source.group_id;
        destination.special_device = source.special_device;
        destination.size = source.size;
        destination.block_size = source.block_size;
        destination.block_count = source.block_count;
        destination.access_time = source.access_time;
        destination.access_time_nanoseconds = source.access_time_nanoseconds;
        destination.modification_time = source.modification_time;
        destination.modification_time_nanoseconds = source.modification_time_nanoseconds;
        destination.status_change_time = source.status_change_time;
        destination.status_change_time_nanoseconds = source.status_change_time_nanoseconds;
        return destination;
    }

    size_t directory_record_size(size_t name_length, bool use_dirent64) noexcept {
        const size_t name_offset = use_dirent64 ? kDirent64NameOffset : kLegacyNameOffset;
        const size_t type_bytes = use_dirent64 ? 0U : 1U;
        if (name_length > std::numeric_limits<uint16_t>::max() - name_offset - 1U - type_bytes) {
            return 0U;
        }
        const size_t unaligned = name_offset + name_length + 1U + type_bytes;
        const size_t aligned =
            (unaligned + kDirectoryRecordAlignment - 1U) & ~(kDirectoryRecordAlignment - 1U);
        return aligned <= std::numeric_limits<uint16_t>::max() ? aligned : 0U;
    }

    bool serialize_directory_record(uint8_t *output, size_t capacity, const char *name,
                                    uint64_t inode, int64_t next_offset, uint8_t type,
                                    bool use_dirent64, size_t &record_size) noexcept {
        record_size = 0U;
        if (output == nullptr || name == nullptr) {
            return false;
        }
        const size_t name_length = string_length(name);
        const size_t required = directory_record_size(name_length, use_dirent64);
        if (required == 0U || required > capacity) {
            return false;
        }

        std::memset(output, 0, required);
        const uint16_t record_length = static_cast<uint16_t>(required);
        std::memcpy(output, &inode, sizeof(inode));
        std::memcpy(output + 8U, &next_offset, sizeof(next_offset));
        std::memcpy(output + 16U, &record_length, sizeof(record_length));
        if (use_dirent64) {
            output[kDirent64TypeOffset] = type;
            std::memcpy(output + kDirent64NameOffset, name, name_length + 1U);
        } else {
            std::memcpy(output + kLegacyNameOffset, name, name_length + 1U);
            output[required - 1U] = type;
        }
        record_size = required;
        return true;
    }

} // namespace xinim::kernel::x86_64

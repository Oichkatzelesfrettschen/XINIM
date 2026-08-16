#include "arch/x86_64/userspace_abi.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

    template <typename Value> Value load_value(const uint8_t *buffer, size_t offset) {
        Value value{};
        std::memcpy(&value, buffer + offset, sizeof(value));
        return value;
    }

    void test_stat_layout_and_conversion() {
        xinim::kernel::bootfs::FileStatus source{};
        source.device = 2U;
        source.inode = 3U;
        source.mode = 0100755U;
        source.link_count = 4U;
        source.user_id = 0x12345678U;
        source.group_id = 0x87654321U;
        source.special_device = 7U;
        source.size = 8193;
        source.block_size = 512;
        source.block_count = 17;
        source.modification_time = 11;

        const auto result = xinim::kernel::x86_64::serialize_stat64(source);
        assert(sizeof(result) == 144U);
        assert(result.device == 2U);
        assert(result.inode == 3U);
        assert(result.mode == 0100755U);
        assert(result.link_count == 4U);
        assert(result.user_id == 0x12345678U);
        assert(result.group_id == 0x87654321U);
        assert(result.size == 8193);
        assert(result.block_count == 17);
        assert(result.modification_time == 11);
    }

    void test_runtime_layouts() {
        using namespace xinim::kernel::x86_64;
        assert(sizeof(UserspaceTimeValue64) == 16U);
        assert(sizeof(UserspaceTimeSpec64) == 16U);
        assert(sizeof(UserspaceResourceLimit64) == 16U);
        assert(sizeof(UserspaceResourceUsage64) == 144U);
        assert(offsetof(UserspaceResourceUsage64, system_time) == 16U);
        assert(offsetof(UserspaceResourceUsage64, counters) == 32U);
    }

    void test_legacy_dirent_layout() {
        uint8_t record[64]{};
        size_t record_size = 0U;
        assert(xinim::kernel::x86_64::serialize_directory_record(record, sizeof(record), "mksh", 9U,
                                                                 4, 8U, false, record_size));
        assert(record_size == 24U);
        assert(load_value<uint64_t>(record, 0U) == 9U);
        assert(load_value<int64_t>(record, 8U) == 4);
        assert(load_value<uint16_t>(record, 16U) == 24U);
        assert(std::strcmp(reinterpret_cast<const char *>(record + 18U), "mksh") == 0);
        assert(record[23U] == 8U);
    }

    void test_dirent64_layout() {
        uint8_t record[64]{};
        size_t record_size = 0U;
        assert(xinim::kernel::x86_64::serialize_directory_record(record, sizeof(record), "bin", 2U,
                                                                 3, 4U, true, record_size));
        assert(record_size == 24U);
        assert(load_value<uint16_t>(record, 16U) == 24U);
        assert(record[18U] == 4U);
        assert(std::strcmp(reinterpret_cast<const char *>(record + 19U), "bin") == 0);
    }

} // namespace

int main() {
    test_runtime_layouts();
    test_stat_layout_and_conversion();
    test_legacy_dirent_layout();
    test_dirent64_layout();
    return 0;
}

// Exercise the production reader and mutation admission against a sector device.
#include "../src/kernel/i486/ext2_reader.cpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <print>

namespace {
using namespace xinim::i486::ext2_reader;
constexpr std::size_t kFixtureBlockSize = 1024U;
constexpr std::size_t kFixtureBlocks = 128U;
constexpr std::size_t kSectorSize = 512U;
constexpr uint32_t kLargeInode = 3U;
constexpr uint32_t kSmallInode = 4U;
constexpr uint32_t kDoubleStart = 12U + 256U;
constexpr uint32_t kTripleStart = kDoubleStart + 256U * 256U;
uint8_t fixture_disk[kFixtureBlocks * kFixtureBlockSize]{};
uint32_t write_count = 0U;
uint32_t failed_sector = UINT32_MAX;

void require(bool condition, const char* description) {
    if (!condition) {
        std::println(stderr, "ext2 indirect contract: {}", description);
        std::abort();
    }
}

bool read_sector(uint32_t sector, uint8_t* buffer) noexcept {
    if (sector == failed_sector || sector >= sizeof(fixture_disk) / kSectorSize) {
        return false;
    }
    std::memcpy(buffer, fixture_disk + sector * kSectorSize, kSectorSize);
    return true;
}

bool write_sector(uint32_t sector, const uint8_t* buffer) noexcept {
    ++write_count;
    if (sector >= sizeof(fixture_disk) / kSectorSize) {
        return false;
    }
    std::memcpy(fixture_disk + sector * kSectorSize, buffer, kSectorSize);
    return true;
}

void store_pointer(uint32_t block, uint32_t index, uint32_t value) {
    std::memcpy(fixture_disk + block * kFixtureBlockSize + index * sizeof(value),
                &value, sizeof(value));
}

void store_inode(uint32_t number, const Ext2Inode& inode) {
    std::memcpy(fixture_disk + 5U * kFixtureBlockSize + (number - 1U) * sizeof(inode),
                &inode, sizeof(inode));
}

void store_dirent(uint32_t offset, uint32_t inode_number, const char* name, uint16_t length) {
    const Ext2DirEntryHeader entry{inode_number, length,
                                  static_cast<uint8_t>(std::strlen(name)), 1U};
    std::memcpy(fixture_disk + 10U * kFixtureBlockSize + offset, &entry, sizeof(entry));
    std::copy_n(name, std::strlen(name),
                fixture_disk + 10U * kFixtureBlockSize + offset + sizeof(entry));
}

Ext2Inode initialize_fixture() {
    std::memset(fixture_disk, 0, sizeof(fixture_disk));
    Ext2Superblock superblock{};
    superblock.magic = kExt2Magic;
    superblock.blocks_count = kFixtureBlocks;
    superblock.inodes_count = 16U;
    superblock.inodes_per_group = 16U;
    superblock.blocks_per_group = kFixtureBlocks;
    superblock.first_data_block = 1U;
    superblock.inode_size = sizeof(Ext2Inode);
    std::memcpy(fixture_disk + 1024U, &superblock, sizeof(superblock));
    Ext2GroupDescriptor group{};
    group.inode_table = 5U;
    group.block_bitmap = 3U;
    group.inode_bitmap = 4U;
    std::memcpy(fixture_disk + 2048U, &group, sizeof(group));
    Ext2Inode directory{};
    directory.mode = kDirectoryType | 0755U;
    directory.size = kFixtureBlockSize;
    directory.block[0] = 10U;
    directory.links_count = 2U;
    store_inode(kRootInodeNumber, directory);
    store_dirent(0U, kLargeInode, "large", 16U);
    store_dirent(16U, kSmallInode, "small", kFixtureBlockSize - 16U);

    Ext2Inode large{};
    large.mode = kRegularFileType | 0644U;
    large.size = (kTripleStart + 65537U) * kFixtureBlockSize;
    large.links_count = 1U;
    large.block[11] = 19U;
    large.block[12] = 20U;
    large.block[13] = 21U;
    large.block[14] = 22U;
    store_pointer(20U, 0U, 24U);
    store_pointer(20U, 255U, 25U);
    store_pointer(21U, 0U, 23U);
    store_pointer(23U, 0U, 26U);
    store_pointer(21U, 1U, 27U);
    store_pointer(27U, 1U, 28U);
    store_pointer(22U, 0U, 29U);
    store_pointer(29U, 0U, 30U);
    store_pointer(30U, 0U, 31U);
    store_pointer(29U, 1U, 32U);
    store_pointer(32U, 1U, 33U);
    for (const uint32_t block : {19U, 24U, 25U, 26U, 28U, 31U, 33U}) {
        std::memset(fixture_disk + block * kFixtureBlockSize, static_cast<int>(block),
                    kFixtureBlockSize);
    }
    store_inode(kLargeInode, large);
    Ext2Inode small{};
    small.mode = kRegularFileType | 0644U;
    small.links_count = 1U;
    small.size = kDoubleStart * kFixtureBlockSize;
    small.block[12] = 34U;
    store_pointer(34U, 255U, 35U);
    store_inode(kSmallInode, small);
    xinim::i486::storage::register_boot_storage(
        {true, 512U, sizeof(fixture_disk) / 512U, "fixture", read_sector, write_sector},
        {true, 0x83U, 0U, sizeof(fixture_disk) / 512U});
    failed_sector = UINT32_MAX;
    write_count = 0U;
    require(load_superblock(), "load actual on-disk superblock");
    return large;
}

void require_byte(uint32_t logical_block, uint8_t expected) {
    uint8_t buffer = 0xFFU;
    require(read_runtime_file("/persist/large", logical_block * kFixtureBlockSize,
                              &buffer, 1U) == 1,
            "public read traverses indirect tree");
    require(buffer == expected, "public read returns selected block or sparse zero");
}

void test_reads() {
    Ext2Inode inode = initialize_fixture();
    require_byte(11U, 19U);
    require_byte(12U, 24U);
    uint8_t boundary[2]{};
    require(read_runtime_file("/persist/large", 274431U, boundary, sizeof(boundary)) == 2,
            "read across the 274432-byte single-indirect boundary");
    require(boundary[0] == 25U && boundary[1] == 26U, "cross-boundary bytes match");
    require_byte(kDoubleStart + 257U, 28U);
    require_byte(kTripleStart, 31U);
    require_byte(kTripleStart + 257U, 33U);
    require_byte(kDoubleStart + 1U, 0U);
    require_byte(kDoubleStart + 512U, 0U);
    require_byte(kTripleStart + 1U, 0U);
    require_byte(kTripleStart + 256U, 0U);
    require_byte(kTripleStart + 65536U, 0U);
    inode.block[14] = 0U;
    store_inode(kLargeInode, inode);
    require_byte(kTripleStart, 0U);
    uint32_t block = 0U;
    require(!get_data_block_number(inode, kTripleStart + 256U * 256U * 256U, block),
            "reject logical index beyond triple-indirect capacity");
}

void test_read_failures() {
    Ext2Inode inode = initialize_fixture();
    uint8_t buffer = 0U;
    for (const uint32_t sector : {42U, 46U, 52U}) {
        failed_sector = sector;
        require(read_runtime_file("/persist/large", 274432U, &buffer, 1U) == -1,
                "propagate root, nested-pointer, and data read failures");
    }
    failed_sector = UINT32_MAX;
    store_pointer(23U, 0U, kFixtureBlocks);
    require(read_runtime_file("/persist/large", 274432U, &buffer, 1U) == -1,
            "reject data pointer beyond filesystem");
    store_pointer(21U, 0U, UINT32_MAX);
    require(read_runtime_file("/persist/large", 274432U, &buffer, 1U) == -1,
            "reject malformed nested pointer before offset multiplication");
    inode.block[13] = kFixtureBlocks;
    store_inode(kLargeInode, inode);
    require(read_runtime_file("/persist/large", 274432U, &buffer, 1U) == -1,
            "reject root pointer beyond filesystem");
    inode.block[0] = kFixtureBlocks;
    uint32_t block = 0U;
    require(!get_data_block_number(inode, 0U, block), "validate direct data pointers too");
}

void test_mutation_admission() {
    Ext2Inode large = initialize_fixture();
    const uint8_t byte = 0x5AU;
    require(write_runtime_file("/persist/large", 0U, &byte, 1U) == -1,
            "reject low-offset writes to indirect-owned large inode");
    require(!truncate_runtime_file("/persist/large", 1U), "reject large truncation");
    require(!unlink_runtime_path("/persist/large"), "reject unlink before dirent removal");
    require(write_count == 0U, "unlink admission precedes directory and bitmap writes");
    require(!rename_runtime_path("/persist/small", "/persist/large"),
            "reject rename replacement before dirent retarget");
    require(write_count == 0U, "rename admission precedes directory and bitmap writes");
    require(!free_inode_storage(large, kLargeInode), "reclamation has its own admission guard");
    require(write_runtime_file("/persist/small", 274431U, &byte, 2U) == -1,
            "reject write spanning supported capacity before changing first byte");
    require(write_runtime_file("/persist/small", UINT32_MAX, &byte, 2U) == -1,
            "reject overflowing offset before gap allocation");
    require(write_count == 0U, "rejected mutations issue zero sector writes");
    require_byte(kDoubleStart, 26U);
    require(write_runtime_file("/persist/small", 274431U, &byte, 1U) == 1,
            "retain last single-indirect byte write");
    uint8_t result = 0U;
    require(read_runtime_file("/persist/small", 274431U, &result, 1U) == 1 && result == byte,
            "supported write persists exact byte");

    large.block[13] = 0U;
    large.block[14] = 0U;
    store_inode(kLargeInode, large);
    write_count = 0U;
    require(!truncate_runtime_file("/persist/large", 0U), "reject large sparse inode mutation");
    require(write_count == 0U, "sparse inode rejection precedes disk writes");

    large.size = 1U;
    for (const uint32_t root_index : {13U, 14U}) {
        large.block[root_index] = 21U;
        store_inode(kLargeInode, large);
        require(write_runtime_file("/persist/large", 0U, &byte, 1U) == -1,
                "high-root ownership rejects mutation even with a small inode size");
        large.block[root_index] = 0U;
    }
    require(write_count == 0U, "high-root rejection precedes disk writes");

    Ext2Inode small{};
    small.mode = kRegularFileType | 0644U;
    small.size = kFixtureBlockSize;
    small.block[0] = 35U;
    store_inode(kSmallInode, small);
    require(write_runtime_file("/persist/small", 0U, &byte, 1U) == 1,
            "retain direct block writes");
    require(truncate_runtime_file("/persist/small", 512U), "retain direct block truncation");
    require(read_runtime_file("/persist/small", 511U, &result, 2U) == 1,
            "truncated inode returns shortened EOF");
}
} // namespace

int main() {
    test_reads();
    test_read_failures();
    test_mutation_admission();
    std::puts("i486 ext2 indirect read and mutation admission contract passed");
}

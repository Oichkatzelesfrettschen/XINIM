#include "ext2_reader.hpp"

#include "bootfs.hpp"
#include "console.hpp"
#include "storage.hpp"

#include <stdint.h>

namespace xinim::i486::ext2_reader {
namespace {

constexpr uint32_t kSectorSize = 512U;
constexpr uint16_t kExt2Magic = 0xEF53U;
constexpr uint32_t kRootInodeNumber = 2U;
constexpr uint16_t kFileTypeMask = 0xF000U;
constexpr uint16_t kDirectoryType = 0x4000U;
constexpr uint16_t kRegularFileType = 0x8000U;
constexpr uint32_t kMaxBlockSize = 4096U;
constexpr uint32_t kMaxInodeSize = 256U;
constexpr uint32_t kMaxPathComponent = 32U;
constexpr uint32_t kMaxDirectoryEntryName = 64U;
constexpr uint32_t kMaxPreviewBytes = 48U;
constexpr uint32_t kMaxPersistPath = 64U;
constexpr uint32_t kExt2DirectBlocks = 12U;
constexpr uint32_t kMaxExecutableBytes = 65536U;
constexpr uint32_t kSingleIndirectIndex = 12U;
constexpr uint32_t kExt2RootPerms = 0755U;
constexpr uint16_t kExt2FtUnknown = 0U;
constexpr uint16_t kExt2FtRegular = 1U;
constexpr uint16_t kExt2FtDirectory = 2U;

struct __attribute__((packed)) Ext2Superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t reserved_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    int32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mount_time;
    uint32_t write_time;
    uint16_t mount_count;
    int16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t last_check;
    uint32_t check_interval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
};

struct __attribute__((packed)) Ext2GroupDescriptor {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint32_t reserved[3];
};

struct __attribute__((packed)) Ext2Inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t osd2[12];
};

struct __attribute__((packed)) Ext2DirEntryHeader {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
};

struct ReaderState {
    bool valid;
    Ext2Superblock superblock;
    Ext2GroupDescriptor group0;
    uint32_t block_size;
    uint32_t inode_size;
} g_state = {false, {}, {}, 0U, 0U};

uint8_t g_sector_buffer[kSectorSize]{};
uint8_t g_block_buffer[kMaxBlockSize]{};
uint8_t g_indirect_block_buffer[kMaxBlockSize]{};
uint8_t g_bitmap_buffer[kMaxBlockSize]{};
char g_preview_buffer[kMaxPreviewBytes + 1U]{};
uint8_t g_executable_buffer[kMaxExecutableBytes]{};
char g_executable_path[kMaxPersistPath]{};
uint32_t g_executable_size = 0U;
bool g_executable_valid = false;

void write_key(const char* key) noexcept {
    console::write_string(key);
    console::write_string(": ");
}

bool read_partition_bytes(uint32_t byte_offset, uint32_t size, uint8_t* buffer) noexcept {
    if (buffer == nullptr || size == 0U) {
        return false;
    }

    const uint32_t start_sector = byte_offset / kSectorSize;
    const uint32_t end_offset = byte_offset + size;
    const uint32_t end_sector = (end_offset - 1U) / kSectorSize;
    uint32_t copied = 0U;

    for (uint32_t sector_index = start_sector; sector_index <= end_sector; ++sector_index) {
        if (!storage::read_boot_partition_sector(sector_index, g_sector_buffer)) {
            return false;
        }

        const uint32_t sector_start = sector_index * kSectorSize;
        const uint32_t copy_begin = byte_offset > sector_start ? byte_offset - sector_start : 0U;
        uint32_t copy_end = kSectorSize;
        if (end_offset < sector_start + kSectorSize) {
            copy_end = end_offset - sector_start;
        }
        const uint32_t copy_size = copy_end - copy_begin;
        __builtin_memcpy(buffer + copied, g_sector_buffer + copy_begin, copy_size);
        copied += copy_size;
    }

    return true;
}

bool write_partition_bytes(uint32_t byte_offset,
                           uint32_t size,
                           const uint8_t* buffer) noexcept {
    if (buffer == nullptr || size == 0U) {
        return false;
    }

    const uint32_t start_sector = byte_offset / kSectorSize;
    const uint32_t end_offset = byte_offset + size;
    const uint32_t end_sector = (end_offset - 1U) / kSectorSize;
    uint32_t copied = 0U;

    for (uint32_t sector_index = start_sector; sector_index <= end_sector; ++sector_index) {
        if (!storage::read_boot_partition_sector(sector_index, g_sector_buffer)) {
            return false;
        }

        const uint32_t sector_start = sector_index * kSectorSize;
        const uint32_t copy_begin = byte_offset > sector_start ? byte_offset - sector_start : 0U;
        uint32_t copy_end = kSectorSize;
        if (end_offset < sector_start + kSectorSize) {
            copy_end = end_offset - sector_start;
        }
        const uint32_t copy_size = copy_end - copy_begin;
        __builtin_memcpy(g_sector_buffer + copy_begin, buffer + copied, copy_size);
        if (!storage::write_boot_partition_sector(sector_index, g_sector_buffer)) {
            return false;
        }
        copied += copy_size;
    }

    return true;
}

bool read_block(uint32_t block_number, uint8_t* buffer) noexcept {
    return read_partition_bytes(block_number * g_state.block_size, g_state.block_size, buffer);
}

bool write_block(uint32_t block_number, const uint8_t* buffer) noexcept {
    return write_partition_bytes(block_number * g_state.block_size, g_state.block_size, buffer);
}

bool write_inode(uint32_t inode_number, const Ext2Inode& inode) noexcept;

bool write_superblock() noexcept {
    return write_partition_bytes(
        1024U,
        sizeof(Ext2Superblock),
        reinterpret_cast<const uint8_t*>(&g_state.superblock));
}

bool write_group_descriptor() noexcept {
    const uint32_t gdt_block = g_state.block_size == 1024U ? 2U : 1U;
    return write_partition_bytes(
        gdt_block * g_state.block_size,
        sizeof(Ext2GroupDescriptor),
        reinterpret_cast<const uint8_t*>(&g_state.group0));
}

uint32_t bytes_to_sectors(uint32_t count) noexcept {
    return (count + kSectorSize - 1U) / kSectorSize;
}

uint16_t directory_record_length(uint8_t name_length) noexcept {
    const uint16_t base = static_cast<uint16_t>(sizeof(Ext2DirEntryHeader) + name_length);
    return static_cast<uint16_t>((base + 3U) & ~3U);
}

bool read_bitmap_block(uint32_t block_number) noexcept {
    return read_block(block_number, g_bitmap_buffer);
}

bool write_bitmap_block(uint32_t block_number) noexcept {
    return write_block(block_number, g_bitmap_buffer);
}

bool test_bitmap_bit(const uint8_t* bitmap, uint32_t bit_index) noexcept {
    if (bitmap == nullptr) {
        return false;
    }
    const uint32_t byte_index = bit_index / 8U;
    const uint32_t bit_offset = bit_index % 8U;
    if (byte_index >= g_state.block_size) {
        return false;
    }
    return (bitmap[byte_index] & static_cast<uint8_t>(1U << bit_offset)) != 0U;
}

void set_bitmap_bit(uint8_t* bitmap, uint32_t bit_index) noexcept {
    if (bitmap == nullptr) {
        return;
    }
    const uint32_t byte_index = bit_index / 8U;
    const uint32_t bit_offset = bit_index % 8U;
    if (byte_index >= g_state.block_size) {
        return;
    }
    bitmap[byte_index] |= static_cast<uint8_t>(1U << bit_offset);
}

void clear_bitmap_bit(uint8_t* bitmap, uint32_t bit_index) noexcept {
    if (bitmap == nullptr) {
        return;
    }
    const uint32_t byte_index = bit_index / 8U;
    const uint32_t bit_offset = bit_index % 8U;
    if (byte_index >= g_state.block_size) {
        return;
    }
    bitmap[byte_index] &= static_cast<uint8_t>(~(1U << bit_offset));
}

bool allocate_group0_block(uint32_t& block_number_out) noexcept {
    block_number_out = 0U;
    if (!read_bitmap_block(g_state.group0.block_bitmap)) {
        return false;
    }

    const uint32_t limit = g_state.superblock.blocks_per_group;
    for (uint32_t bit_index = 0U; bit_index < limit; ++bit_index) {
        if (test_bitmap_bit(g_bitmap_buffer, bit_index)) {
            continue;
        }
        set_bitmap_bit(g_bitmap_buffer, bit_index);
        if (!write_bitmap_block(g_state.group0.block_bitmap)) {
            return false;
        }

        if (g_state.superblock.free_blocks_count != 0U) {
            --g_state.superblock.free_blocks_count;
        }
        if (g_state.group0.free_blocks_count != 0U) {
            --g_state.group0.free_blocks_count;
        }
        if (!write_superblock() || !write_group_descriptor()) {
            return false;
        }

        block_number_out = g_state.superblock.first_data_block + bit_index;
        __builtin_memset(g_block_buffer, 0, g_state.block_size);
        return write_block(block_number_out, g_block_buffer);
    }

    return false;
}

bool free_group0_block(uint32_t block_number) noexcept {
    if (block_number < g_state.superblock.first_data_block) {
        return false;
    }
    const uint32_t bit_index = block_number - g_state.superblock.first_data_block;
    if (bit_index >= g_state.superblock.blocks_per_group) {
        return false;
    }
    if (!read_bitmap_block(g_state.group0.block_bitmap)) {
        return false;
    }
    if (!test_bitmap_bit(g_bitmap_buffer, bit_index)) {
        return false;
    }
    clear_bitmap_bit(g_bitmap_buffer, bit_index);
    if (!write_bitmap_block(g_state.group0.block_bitmap)) {
        return false;
    }
    ++g_state.superblock.free_blocks_count;
    ++g_state.group0.free_blocks_count;
    return write_superblock() && write_group_descriptor();
}

bool allocate_group0_inode(bool is_directory, uint32_t& inode_number_out) noexcept {
    inode_number_out = 0U;
    if (!read_bitmap_block(g_state.group0.inode_bitmap)) {
        return false;
    }

    const uint32_t limit = g_state.superblock.inodes_per_group;
    for (uint32_t bit_index = 0U; bit_index < limit; ++bit_index) {
        if (test_bitmap_bit(g_bitmap_buffer, bit_index)) {
            continue;
        }
        set_bitmap_bit(g_bitmap_buffer, bit_index);
        if (!write_bitmap_block(g_state.group0.inode_bitmap)) {
            return false;
        }

        if (g_state.superblock.free_inodes_count != 0U) {
            --g_state.superblock.free_inodes_count;
        }
        if (g_state.group0.free_inodes_count != 0U) {
            --g_state.group0.free_inodes_count;
        }
        if (is_directory) {
            ++g_state.group0.used_dirs_count;
        }
        if (!write_superblock() || !write_group_descriptor()) {
            return false;
        }

        inode_number_out = bit_index + 1U;
        return true;
    }

    return false;
}

bool free_group0_inode(uint32_t inode_number, bool is_directory) noexcept {
    if (inode_number == 0U) {
        return false;
    }
    const uint32_t bit_index = inode_number - 1U;
    if (bit_index >= g_state.superblock.inodes_per_group) {
        return false;
    }
    if (!read_bitmap_block(g_state.group0.inode_bitmap)) {
        return false;
    }
    if (!test_bitmap_bit(g_bitmap_buffer, bit_index)) {
        return false;
    }
    clear_bitmap_bit(g_bitmap_buffer, bit_index);
    if (!write_bitmap_block(g_state.group0.inode_bitmap)) {
        return false;
    }
    ++g_state.superblock.free_inodes_count;
    ++g_state.group0.free_inodes_count;
    if (is_directory && g_state.group0.used_dirs_count != 0U) {
        --g_state.group0.used_dirs_count;
    }
    return write_superblock() && write_group_descriptor();
}

bool get_data_block_number(const Ext2Inode& inode,
                           uint32_t logical_block_index,
                           uint32_t& block_number_out) noexcept {
    block_number_out = 0U;
    if (logical_block_index < kExt2DirectBlocks) {
        block_number_out = inode.block[logical_block_index];
        return true;
    }

    const uint32_t entries_per_indirect_block = g_state.block_size / sizeof(uint32_t);
    const uint32_t indirect_index = logical_block_index - kExt2DirectBlocks;
    if (entries_per_indirect_block == 0U || indirect_index >= entries_per_indirect_block) {
        return false;
    }

    const uint32_t indirect_block = inode.block[kSingleIndirectIndex];
    if (indirect_block == 0U) {
        return true;
    }
    if (!read_block(indirect_block, g_indirect_block_buffer)) {
        return false;
    }

    const auto* entries = reinterpret_cast<const uint32_t*>(g_indirect_block_buffer);
    block_number_out = entries[indirect_index];
    return true;
}

bool set_data_block_number(Ext2Inode& inode,
                           uint32_t inode_number,
                           uint32_t logical_block_index,
                           uint32_t block_number) noexcept {
    if (logical_block_index < kExt2DirectBlocks) {
        inode.block[logical_block_index] = block_number;
        return write_inode(inode_number, inode);
    }

    const uint32_t indirect_index = logical_block_index - kExt2DirectBlocks;
    if (indirect_index >= (g_state.block_size / sizeof(uint32_t))) {
        return false;
    }

    if (inode.block[kSingleIndirectIndex] == 0U) {
        uint32_t indirect_block = 0U;
        if (!allocate_group0_block(indirect_block)) {
            return false;
        }
        inode.block[kSingleIndirectIndex] = indirect_block;
        inode.blocks += bytes_to_sectors(g_state.block_size);
        if (!write_inode(inode_number, inode)) {
            return false;
        }
        __builtin_memset(g_indirect_block_buffer, 0, g_state.block_size);
        if (!write_block(indirect_block, g_indirect_block_buffer)) {
            return false;
        }
    }

    if (!read_block(inode.block[kSingleIndirectIndex], g_indirect_block_buffer)) {
        return false;
    }
    auto* entries = reinterpret_cast<uint32_t*>(g_indirect_block_buffer);
    entries[indirect_index] = block_number;
    return write_block(inode.block[kSingleIndirectIndex], g_indirect_block_buffer);
}

bool ensure_inode_data_block(Ext2Inode& inode,
                             uint32_t inode_number,
                             uint32_t logical_block_index,
                             uint32_t& block_number_out) noexcept {
    if (!get_data_block_number(inode, logical_block_index, block_number_out)) {
        return false;
    }
    if (block_number_out != 0U) {
        return true;
    }
    if (!allocate_group0_block(block_number_out)) {
        return false;
    }
    if (!set_data_block_number(inode, inode_number, logical_block_index, block_number_out)) {
        return false;
    }
    inode.blocks += bytes_to_sectors(g_state.block_size);
    return write_inode(inode_number, inode);
}

bool load_superblock() noexcept {
    if (!read_partition_bytes(1024U, sizeof(Ext2Superblock),
                              reinterpret_cast<uint8_t*>(&g_state.superblock))) {
        return false;
    }

    if (g_state.superblock.magic != kExt2Magic) {
        return false;
    }

    g_state.block_size = 1024U << g_state.superblock.log_block_size;
    g_state.inode_size = g_state.superblock.inode_size == 0U ? 128U : g_state.superblock.inode_size;
    if (g_state.block_size == 0U || g_state.block_size > kMaxBlockSize ||
        g_state.inode_size == 0U || g_state.inode_size > kMaxInodeSize) {
        return false;
    }

    const uint32_t gdt_block = g_state.block_size == 1024U ? 2U : 1U;
    if (!read_partition_bytes(gdt_block * g_state.block_size, sizeof(Ext2GroupDescriptor),
                              reinterpret_cast<uint8_t*>(&g_state.group0))) {
        return false;
    }

    g_state.valid = true;
    return true;
}

bool read_inode(uint32_t inode_number, Ext2Inode& inode) noexcept {
    if (!g_state.valid || inode_number == 0U) {
        return false;
    }

    const uint32_t group = (inode_number - 1U) / g_state.superblock.inodes_per_group;
    if (group != 0U) {
        return false;
    }

    const uint32_t index = (inode_number - 1U) % g_state.superblock.inodes_per_group;
    const uint32_t inode_table_block = g_state.group0.inode_table;
    const uint32_t inode_byte_offset = index * g_state.inode_size;
    const uint32_t absolute_offset = (inode_table_block * g_state.block_size) + inode_byte_offset;
    return read_partition_bytes(absolute_offset, sizeof(Ext2Inode),
                                reinterpret_cast<uint8_t*>(&inode));
}

bool inode_absolute_offset(uint32_t inode_number, uint32_t& absolute_offset) noexcept {
    if (!g_state.valid || inode_number == 0U) {
        return false;
    }
    const uint32_t group = (inode_number - 1U) / g_state.superblock.inodes_per_group;
    if (group != 0U) {
        return false;
    }
    const uint32_t index = (inode_number - 1U) % g_state.superblock.inodes_per_group;
    const uint32_t inode_table_block = g_state.group0.inode_table;
    const uint32_t inode_byte_offset = index * g_state.inode_size;
    absolute_offset = (inode_table_block * g_state.block_size) + inode_byte_offset;
    return true;
}

bool write_inode(uint32_t inode_number, const Ext2Inode& inode) noexcept {
    uint32_t absolute_offset = 0U;
    if (!inode_absolute_offset(inode_number, absolute_offset)) {
        return false;
    }
    return write_partition_bytes(absolute_offset,
                                 sizeof(Ext2Inode),
                                 reinterpret_cast<const uint8_t*>(&inode));
}

bool name_equals(const char* lhs, const char* rhs, uint32_t length) noexcept {
    for (uint32_t index = 0U; index < length; ++index) {
        if (lhs[index] != rhs[index]) {
            return false;
        }
    }
    return rhs[length] == '\0';
}

uint32_t string_length(const char* text) noexcept {
    uint32_t length = 0U;
    if (text == nullptr) {
        return 0U;
    }
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    uint32_t index = 0U;
    for (;; ++index) {
        if (lhs[index] != rhs[index]) {
            return false;
        }
        if (lhs[index] == '\0') {
            return true;
        }
    }
}

bool starts_with(const char* text, const char* prefix) noexcept {
    if (text == nullptr || prefix == nullptr) {
        return false;
    }
    uint32_t index = 0U;
    while (prefix[index] != '\0') {
        if (text[index] != prefix[index]) {
            return false;
        }
        ++index;
    }
    return true;
}

bool path_is_at_or_below(const char* path, const char* ancestor) noexcept {
    if (path == nullptr || ancestor == nullptr) {
        return false;
    }
    if (!starts_with(path, ancestor)) {
        return false;
    }
    const uint32_t ancestor_length = string_length(ancestor);
    if (ancestor_length == 0U) {
        return false;
    }
    if (ancestor[ancestor_length - 1U] == '/') {
        return true;
    }
    const char suffix = path[ancestor_length];
    return suffix == '\0' || suffix == '/';
}

bool map_runtime_path(const char* path, char* ext2_path, uint32_t capacity) noexcept {
    if (path == nullptr || ext2_path == nullptr || capacity < 2U) {
        return false;
    }
    if (starts_with(path, "/persist")) {
        const char suffix = path[8];
        if (suffix != '\0' && suffix != '/') {
            return false;
        }

        if (suffix == '\0') {
            ext2_path[0] = '/';
            ext2_path[1] = '\0';
            return true;
        }

        const uint32_t source_length = string_length(path + 8U);
        if (source_length + 1U >= capacity) {
            return false;
        }
        for (uint32_t index = 0U; index <= source_length; ++index) {
            ext2_path[index] = path[8U + index];
        }
        return true;
    }

    static constexpr const char* kCanonicalPrefixes[] = {
        "/bin",
        "/etc",
    };
    for (const char* prefix : kCanonicalPrefixes) {
        const uint32_t prefix_length = string_length(prefix);
        if (!starts_with(path, prefix)) {
            continue;
        }
        const char suffix = path[prefix_length];
        if (suffix != '\0' && suffix != '/') {
            continue;
        }
        const uint32_t source_length = string_length(path);
        if (source_length + 1U >= capacity) {
            return false;
        }
        for (uint32_t index = 0U; index <= source_length; ++index) {
            ext2_path[index] = path[index];
        }
        return true;
    }
    return false;
}

void cache_executable_path(const char* path) noexcept {
    const uint32_t length = string_length(path);
    for (uint32_t index = 0U; index < sizeof(g_executable_path); ++index) {
        g_executable_path[index] = index < length ? path[index] : '\0';
        if (index >= length) {
            break;
        }
    }
}

bool lookup_in_directory(const Ext2Inode& directory,
                         const char* name,
                         uint32_t& inode_number_out) noexcept {
    if ((directory.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    const uint32_t total_blocks =
        (directory.size + g_state.block_size - 1U) / g_state.block_size;
    for (uint32_t block_index = 0U; block_index < total_blocks; ++block_index) {
        uint32_t data_block = 0U;
        if (!get_data_block_number(directory, block_index, data_block)) {
            return false;
        }
        if (data_block == 0U) {
            continue;
        }
        if (!read_block(data_block, g_block_buffer)) {
            return false;
        }

        uint32_t offset = 0U;
        while (offset + sizeof(Ext2DirEntryHeader) <= g_state.block_size) {
            const auto* entry = reinterpret_cast<const Ext2DirEntryHeader*>(g_block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2DirEntryHeader) || entry->rec_len == 0U) {
                break;
            }
            if (entry->inode != 0U &&
                entry->name_len <= kMaxPathComponent &&
                offset + entry->rec_len <= g_state.block_size) {
                const char* entry_name =
                    reinterpret_cast<const char*>(g_block_buffer + offset + sizeof(Ext2DirEntryHeader));
                if (name_equals(entry_name, name, entry->name_len)) {
                    inode_number_out = entry->inode;
                    return true;
                }
            }
            offset += entry->rec_len;
        }
    }

    return false;
}

bool read_inode_range(const Ext2Inode& inode,
                      uint32_t offset,
                      uint8_t* buffer,
                      uint32_t count,
                      uint32_t& size_out) noexcept {
    size_out = 0U;
    if ((inode.mode & kFileTypeMask) != kRegularFileType || buffer == nullptr || count == 0U) {
        return false;
    }
    if (offset >= inode.size) {
        return true;
    }

    uint32_t remaining = inode.size - offset;
    if (count > remaining) {
        count = remaining;
    }

    while (size_out < count) {
        const uint32_t absolute_offset = offset + size_out;
        const uint32_t block_index = absolute_offset / g_state.block_size;
        uint32_t block_number = 0U;
        if (!get_data_block_number(inode, block_index, block_number)) {
            return false;
        }

        const uint32_t offset_in_block = absolute_offset % g_state.block_size;
        uint32_t chunk = g_state.block_size - offset_in_block;
        if (chunk > (count - size_out)) {
            chunk = count - size_out;
        }

        if (block_number == 0U) {
            __builtin_memset(buffer + size_out, 0, chunk);
        } else {
            if (!read_block(block_number, g_block_buffer)) {
                return false;
            }
            __builtin_memcpy(buffer + size_out, g_block_buffer + offset_in_block, chunk);
        }
        size_out += chunk;
    }

    return true;
}

using DirectoryVisitor = bool (*)(const Ext2DirEntryHeader& entry,
                                  const char* name,
                                  bool is_directory,
                                  void* context) noexcept;

bool visit_directory_entries(const Ext2Inode& directory,
                             DirectoryVisitor visitor,
                             void* context) noexcept {
    if ((directory.mode & kFileTypeMask) != kDirectoryType || visitor == nullptr) {
        return false;
    }

    const uint32_t total_blocks =
        (directory.size + g_state.block_size - 1U) / g_state.block_size;
    for (uint32_t block_index = 0U; block_index < total_blocks; ++block_index) {
        uint32_t data_block = 0U;
        if (!get_data_block_number(directory, block_index, data_block)) {
            return false;
        }
        if (data_block == 0U) {
            continue;
        }
        if (!read_block(data_block, g_block_buffer)) {
            return false;
        }

        uint32_t offset = 0U;
        while (offset + sizeof(Ext2DirEntryHeader) <= g_state.block_size) {
            const auto* entry = reinterpret_cast<const Ext2DirEntryHeader*>(g_block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2DirEntryHeader) || entry->rec_len == 0U) {
                break;
            }
            if (entry->inode != 0U &&
                entry->name_len != 0U &&
                entry->name_len <= kMaxDirectoryEntryName &&
                offset + entry->rec_len <= g_state.block_size) {
                char name[kMaxDirectoryEntryName + 1U]{};
                const char* source =
                    reinterpret_cast<const char*>(g_block_buffer + offset + sizeof(Ext2DirEntryHeader));
                __builtin_memcpy(name, source, entry->name_len);
                name[entry->name_len] = '\0';

                bool is_directory = entry->file_type == 2U;
                if (entry->file_type == 0U) {
                    Ext2Inode child{};
                    if (read_inode(entry->inode, child)) {
                        is_directory = (child.mode & kFileTypeMask) == kDirectoryType;
                    }
                }

                if (!visitor(*entry, name, is_directory, context)) {
                    return true;
                }
            }
            offset += entry->rec_len;
        }
    }

    return true;
}

bool read_small_file(const Ext2Inode& inode, char* preview, uint32_t preview_capacity) noexcept {
    if ((inode.mode & kFileTypeMask) != kRegularFileType || preview == nullptr ||
        preview_capacity == 0U) {
        return false;
    }

    const uint32_t data_block = inode.block[0];
    if (data_block == 0U || !read_block(data_block, g_block_buffer)) {
        return false;
    }

    const uint32_t copy_size =
        inode.size < (preview_capacity - 1U) ? inode.size : (preview_capacity - 1U);
    __builtin_memcpy(preview, g_block_buffer, copy_size);
    preview[copy_size] = '\0';
    return true;
}

void sanitize_preview(char* text) noexcept {
    for (uint32_t index = 0U; text[index] != '\0'; ++index) {
        const unsigned char value = static_cast<unsigned char>(text[index]);
        if (value < 32U || value > 126U) {
            text[index] = '.';
        }
    }
}

void log_root_inode(const Ext2Inode& inode) noexcept {
    write_key("ext2 root inode");
    console::write_string("mode=");
    console::write_hex32(inode.mode);
    console::write_string(" size=");
    console::write_dec32(inode.size);
    console::newline();
}

void log_persist_file_probe() noexcept {
    Ext2Inode root{};
    if (!read_inode(kRootInodeNumber, root)) {
        write_key("ext2 /etc/persist.txt");
        console::write_string("root inode read failed");
        console::newline();
        return;
    }

    log_root_inode(root);

    uint32_t etc_inode_number = 0U;
    if (!lookup_in_directory(root, "etc", etc_inode_number)) {
        write_key("ext2 /etc");
        console::write_string("missing");
        console::newline();
        return;
    }

    Ext2Inode etc_inode{};
    if (!read_inode(etc_inode_number, etc_inode)) {
        write_key("ext2 /etc");
        console::write_string("inode read failed");
        console::newline();
        return;
    }

    uint32_t persist_inode_number = 0U;
    if (!lookup_in_directory(etc_inode, "persist.txt", persist_inode_number)) {
        write_key("ext2 /etc/persist.txt");
        console::write_string("missing");
        console::newline();
        return;
    }

    Ext2Inode persist_inode{};
    if (!read_inode(persist_inode_number, persist_inode)) {
        write_key("ext2 /etc/persist.txt");
        console::write_string("inode read failed");
        console::newline();
        return;
    }

    __builtin_memset(g_preview_buffer, 0, sizeof(g_preview_buffer));
    if (!read_small_file(persist_inode, g_preview_buffer, sizeof(g_preview_buffer))) {
        write_key("ext2 /etc/persist.txt");
        console::write_string("read failed");
        console::newline();
        return;
    }

    sanitize_preview(g_preview_buffer);
    write_key("ext2 /etc/persist.txt");
    console::write_string(g_preview_buffer);
    console::newline();
}

bool resolve_path(const char* path, Ext2Inode& inode_out) noexcept {
    if (!g_state.valid || path == nullptr || path[0] != '/') {
        return false;
    }

    Ext2Inode current{};
    if (!read_inode(kRootInodeNumber, current)) {
        return false;
    }
    if (path[1] == '\0') {
        inode_out = current;
        return true;
    }

    uint32_t cursor = 1U;
    while (path[cursor] != '\0') {
        while (path[cursor] == '/') {
            ++cursor;
        }
        if (path[cursor] == '\0') {
            break;
        }

        char component[kMaxPathComponent + 1U]{};
        uint32_t component_length = 0U;
        while (path[cursor] != '\0' && path[cursor] != '/') {
            if (component_length >= kMaxPathComponent) {
                return false;
            }
            component[component_length++] = path[cursor++];
        }
        component[component_length] = '\0';

        uint32_t child_inode_number = 0U;
        if (!lookup_in_directory(current, component, child_inode_number)) {
            return false;
        }
        if (!read_inode(child_inode_number, current)) {
            return false;
        }
    }

    inode_out = current;
    return true;
}

bool resolve_path_with_inode_number(const char* path,
                                    Ext2Inode& inode_out,
                                    uint32_t& inode_number_out) noexcept {
    if (!g_state.valid || path == nullptr || path[0] != '/') {
        return false;
    }

    Ext2Inode current{};
    if (!read_inode(kRootInodeNumber, current)) {
        return false;
    }
    uint32_t current_inode_number = kRootInodeNumber;

    if (path[1] == '\0') {
        inode_out = current;
        inode_number_out = current_inode_number;
        return true;
    }

    uint32_t cursor = 1U;
    while (path[cursor] != '\0') {
        while (path[cursor] == '/') {
            ++cursor;
        }
        if (path[cursor] == '\0') {
            break;
        }

        char component[kMaxPathComponent + 1U]{};
        uint32_t component_length = 0U;
        while (path[cursor] != '\0' && path[cursor] != '/') {
            if (component_length >= kMaxPathComponent) {
                return false;
            }
            component[component_length++] = path[cursor++];
        }
        component[component_length] = '\0';

        uint32_t child_inode_number = 0U;
        if (!lookup_in_directory(current, component, child_inode_number)) {
            return false;
        }
        if (!read_inode(child_inode_number, current)) {
            return false;
        }
        current_inode_number = child_inode_number;
    }

    inode_out = current;
    inode_number_out = current_inode_number;
    return true;
}

bool split_parent_path(const char* path,
                       char* parent_path,
                       uint32_t parent_capacity,
                       char* leaf_name,
                       uint32_t leaf_capacity) noexcept {
    if (path == nullptr || parent_path == nullptr || leaf_name == nullptr ||
        parent_capacity < 2U || leaf_capacity < 2U || path[0] != '/') {
        return false;
    }

    uint32_t length = string_length(path);
    while (length > 1U && path[length - 1U] == '/') {
        --length;
    }
    if (length <= 1U) {
        return false;
    }

    uint32_t slash = length;
    while (slash > 0U && path[slash - 1U] != '/') {
        --slash;
    }
    if (slash == 0U || slash >= length) {
        return false;
    }

    const uint32_t leaf_length = length - slash;
    if (leaf_length == 0U || leaf_length + 1U > leaf_capacity) {
        return false;
    }
    for (uint32_t index = 0U; index < leaf_length; ++index) {
        leaf_name[index] = path[slash + index];
    }
    leaf_name[leaf_length] = '\0';
    if (string_equals(leaf_name, ".") || string_equals(leaf_name, "..")) {
        return false;
    }

    if (slash == 1U) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
        return true;
    }
    if (slash + 1U > parent_capacity) {
        return false;
    }
    for (uint32_t index = 0U; index < slash; ++index) {
        parent_path[index] = path[index];
    }
    parent_path[slash] = '\0';
    return true;
}

bool add_directory_entry(Ext2Inode& directory_inode,
                         uint32_t directory_inode_number,
                         const char* name,
                         uint32_t child_inode_number,
                         uint8_t file_type) noexcept {
    if ((directory_inode.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }
    const uint32_t name_length = string_length(name);
    if (name_length == 0U || name_length > kMaxDirectoryEntryName) {
        return false;
    }

    const uint16_t required_len = directory_record_length(static_cast<uint8_t>(name_length));
    const uint32_t dir_blocks =
        directory_inode.size == 0U ? 0U
                                   : ((directory_inode.size + g_state.block_size - 1U) /
                                      g_state.block_size);

    for (uint32_t block_index = 0U; block_index < dir_blocks; ++block_index) {
        uint32_t block_number = 0U;
        if (!get_data_block_number(directory_inode, block_index, block_number) || block_number == 0U) {
            continue;
        }
        if (!read_block(block_number, g_block_buffer)) {
            return false;
        }

        uint32_t offset = 0U;
        while (offset + sizeof(Ext2DirEntryHeader) <= g_state.block_size) {
            auto* entry = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2DirEntryHeader) || entry->rec_len == 0U) {
                break;
            }

            const uint16_t actual_len = entry->inode == 0U
                ? required_len
                : directory_record_length(entry->name_len);
            if (entry->inode == 0U && entry->rec_len >= required_len) {
                entry->inode = child_inode_number;
                entry->name_len = static_cast<uint8_t>(name_length);
                entry->file_type = file_type;
                __builtin_memcpy(
                    g_block_buffer + offset + sizeof(Ext2DirEntryHeader),
                    name,
                    name_length);
                return write_block(block_number, g_block_buffer);
            }

            if (entry->inode != 0U && entry->rec_len >= actual_len &&
                entry->rec_len - actual_len >= required_len) {
                const uint16_t original_len = entry->rec_len;
                entry->rec_len = actual_len;
                auto* new_entry = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + offset + actual_len);
                new_entry->inode = child_inode_number;
                new_entry->rec_len = static_cast<uint16_t>(original_len - actual_len);
                new_entry->name_len = static_cast<uint8_t>(name_length);
                new_entry->file_type = file_type;
                __builtin_memcpy(
                    reinterpret_cast<uint8_t*>(new_entry) + sizeof(Ext2DirEntryHeader),
                    name,
                    name_length);
                return write_block(block_number, g_block_buffer);
            }
            offset += entry->rec_len;
        }
    }

    uint32_t new_block = 0U;
    if (!allocate_group0_block(new_block)) {
        return false;
    }
    const uint32_t new_block_index = dir_blocks;
    if (new_block_index >= kExt2DirectBlocks) {
        (void)free_group0_block(new_block);
        return false;
    }
    directory_inode.block[new_block_index] = new_block;
    directory_inode.size += g_state.block_size;
    directory_inode.blocks += bytes_to_sectors(g_state.block_size);
    if (!write_inode(directory_inode_number, directory_inode)) {
        (void)free_group0_block(new_block);
        return false;
    }

    __builtin_memset(g_block_buffer, 0, g_state.block_size);
    auto* entry = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer);
    entry->inode = child_inode_number;
    entry->rec_len = static_cast<uint16_t>(g_state.block_size);
    entry->name_len = static_cast<uint8_t>(name_length);
    entry->file_type = file_type;
    __builtin_memcpy(g_block_buffer + sizeof(Ext2DirEntryHeader), name, name_length);
    return write_block(new_block, g_block_buffer);
}

bool remove_directory_entry(Ext2Inode& directory_inode,
                            uint32_t directory_inode_number,
                            const char* name,
                            uint32_t& child_inode_number_out,
                            bool& child_is_directory_out) noexcept {
    (void)directory_inode_number;
    child_inode_number_out = 0U;
    child_is_directory_out = false;
    if ((directory_inode.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    const uint32_t dir_blocks =
        (directory_inode.size + g_state.block_size - 1U) / g_state.block_size;
    for (uint32_t block_index = 0U; block_index < dir_blocks; ++block_index) {
        uint32_t block_number = 0U;
        if (!get_data_block_number(directory_inode, block_index, block_number) || block_number == 0U) {
            continue;
        }
        if (!read_block(block_number, g_block_buffer)) {
            return false;
        }

        uint32_t offset = 0U;
        uint32_t previous_offset = 0U;
        Ext2DirEntryHeader* previous = nullptr;
        while (offset + sizeof(Ext2DirEntryHeader) <= g_state.block_size) {
            auto* entry = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2DirEntryHeader) || entry->rec_len == 0U) {
                break;
            }
            if (entry->inode != 0U &&
                entry->name_len <= kMaxDirectoryEntryName &&
                name_equals(
                    reinterpret_cast<const char*>(g_block_buffer + offset + sizeof(Ext2DirEntryHeader)),
                    name,
                    entry->name_len)) {
                child_inode_number_out = entry->inode;
                child_is_directory_out = entry->file_type == kExt2FtDirectory;
                if (entry->file_type == kExt2FtUnknown) {
                    Ext2Inode child{};
                    if (read_inode(entry->inode, child)) {
                        child_is_directory_out = (child.mode & kFileTypeMask) == kDirectoryType;
                    }
                }

                if (previous != nullptr) {
                    previous->rec_len = static_cast<uint16_t>(previous->rec_len + entry->rec_len);
                } else {
                    entry->inode = 0U;
                }
                return write_block(block_number, g_block_buffer);
            }
            previous_offset = offset;
            previous = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + previous_offset);
            offset += entry->rec_len;
        }
    }

    return false;
}

bool directory_is_empty(const Ext2Inode& inode) noexcept {
    struct EmptyContext {
        bool empty;
    } context{true};

    auto visitor = [](const Ext2DirEntryHeader&, const char* name, bool, void* raw) noexcept -> bool {
        auto* context = static_cast<EmptyContext*>(raw);
        if (context == nullptr) {
            return false;
        }
        if (!string_equals(name, ".") && !string_equals(name, "..")) {
            context->empty = false;
            return false;
        }
        return true;
    };

    if (!visit_directory_entries(inode, visitor, &context)) {
        return false;
    }
    return context.empty;
}

bool free_inode_storage(Ext2Inode& inode, uint32_t inode_number) noexcept {
    for (uint32_t index = 0U; index < kExt2DirectBlocks; ++index) {
        if (inode.block[index] != 0U) {
            if (!free_group0_block(inode.block[index])) {
                return false;
            }
            inode.block[index] = 0U;
        }
    }

    if (inode.block[kSingleIndirectIndex] != 0U) {
        if (!read_block(inode.block[kSingleIndirectIndex], g_indirect_block_buffer)) {
            return false;
        }
        const auto* entries = reinterpret_cast<const uint32_t*>(g_indirect_block_buffer);
        const uint32_t entry_count = g_state.block_size / sizeof(uint32_t);
        for (uint32_t index = 0U; index < entry_count; ++index) {
            if (entries[index] != 0U && !free_group0_block(entries[index])) {
                return false;
            }
        }
        if (!free_group0_block(inode.block[kSingleIndirectIndex])) {
            return false;
        }
        inode.block[kSingleIndirectIndex] = 0U;
    }

    inode.size = 0U;
    inode.dir_acl = 0U;
    inode.blocks = 0U;
    return write_inode(inode_number, inode);
}

bool create_node(const char* path, uint16_t mode, bool directory) noexcept {
    char parent_path[kMaxPersistPath]{};
    char leaf_name[kMaxPathComponent + 1U]{};
    if (!split_parent_path(path, parent_path, sizeof(parent_path), leaf_name, sizeof(leaf_name))) {
        return false;
    }

    Ext2Inode parent{};
    uint32_t parent_inode_number = 0U;
    if (!resolve_path_with_inode_number(parent_path, parent, parent_inode_number) ||
        (parent.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    uint32_t existing_inode_number = 0U;
    if (lookup_in_directory(parent, leaf_name, existing_inode_number)) {
        return false;
    }

    uint32_t inode_number = 0U;
    if (!allocate_group0_inode(directory, inode_number)) {
        return false;
    }

    Ext2Inode inode{};
    inode.mode = static_cast<uint16_t>((directory ? kDirectoryType : kRegularFileType) | (mode & 0777U));
    inode.links_count = static_cast<uint16_t>(directory ? 2U : 1U);
    inode.size = 0U;
    inode.blocks = 0U;
    inode.dir_acl = 0U;

    if (directory) {
        uint32_t block_number = 0U;
        if (!allocate_group0_block(block_number)) {
            (void)free_group0_inode(inode_number, true);
            return false;
        }
        inode.block[0] = block_number;
        inode.size = g_state.block_size;
        inode.blocks = bytes_to_sectors(g_state.block_size);
        if (!write_inode(inode_number, inode)) {
            (void)free_group0_block(block_number);
            (void)free_group0_inode(inode_number, true);
            return false;
        }

        __builtin_memset(g_block_buffer, 0, g_state.block_size);
        auto* dot = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer);
        dot->inode = inode_number;
        dot->rec_len = directory_record_length(1U);
        dot->name_len = 1U;
        dot->file_type = kExt2FtDirectory;
        g_block_buffer[sizeof(Ext2DirEntryHeader)] = '.';

        auto* dotdot = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + dot->rec_len);
        dotdot->inode = parent_inode_number;
        dotdot->rec_len = static_cast<uint16_t>(g_state.block_size - dot->rec_len);
        dotdot->name_len = 2U;
        dotdot->file_type = kExt2FtDirectory;
        auto* dotdot_name = reinterpret_cast<char*>(g_block_buffer + dot->rec_len + sizeof(Ext2DirEntryHeader));
        dotdot_name[0] = '.';
        dotdot_name[1] = '.';
        if (!write_block(block_number, g_block_buffer)) {
            (void)free_group0_block(block_number);
            (void)free_group0_inode(inode_number, true);
            return false;
        }
    } else if (!write_inode(inode_number, inode)) {
        (void)free_group0_inode(inode_number, false);
        return false;
    }

    const uint8_t entry_type = directory ? kExt2FtDirectory : kExt2FtRegular;
    if (!add_directory_entry(parent, parent_inode_number, leaf_name, inode_number, entry_type)) {
        if (directory && inode.block[0] != 0U) {
            (void)free_group0_block(inode.block[0]);
        }
        (void)free_group0_inode(inode_number, directory);
        return false;
    }

    if (directory) {
        ++parent.links_count;
        if (!write_inode(parent_inode_number, parent)) {
            return false;
        }
    }

    g_executable_valid = false;
    return true;
}

uint8_t inode_dirent_type(const Ext2Inode& inode) noexcept {
    const uint16_t type = inode.mode & kFileTypeMask;
    if (type == kDirectoryType) {
        return kExt2FtDirectory;
    }
    if (type == kRegularFileType) {
        return kExt2FtRegular;
    }
    return kExt2FtUnknown;
}

bool rename_directory_entry_in_place(Ext2Inode& directory_inode,
                                     const char* old_name,
                                     const char* new_name) noexcept {
    if ((directory_inode.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }
    const uint32_t new_name_length = string_length(new_name);
    if (new_name_length == 0U || new_name_length > kMaxDirectoryEntryName) {
        return false;
    }

    const uint32_t dir_blocks =
        (directory_inode.size + g_state.block_size - 1U) / g_state.block_size;
    for (uint32_t block_index = 0U; block_index < dir_blocks; ++block_index) {
        uint32_t block_number = 0U;
        if (!get_data_block_number(directory_inode, block_index, block_number) || block_number == 0U) {
            continue;
        }
        if (!read_block(block_number, g_block_buffer)) {
            return false;
        }

        uint32_t offset = 0U;
        while (offset + sizeof(Ext2DirEntryHeader) <= g_state.block_size) {
            auto* entry = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2DirEntryHeader) || entry->rec_len == 0U) {
                break;
            }
            if (entry->inode != 0U &&
                entry->name_len <= kMaxDirectoryEntryName &&
                name_equals(
                    reinterpret_cast<const char*>(g_block_buffer + offset + sizeof(Ext2DirEntryHeader)),
                    old_name,
                    entry->name_len)) {
                const uint32_t max_name_bytes = entry->rec_len - sizeof(Ext2DirEntryHeader);
                if (new_name_length > max_name_bytes) {
                    return false;
                }
                auto* name_bytes = g_block_buffer + offset + sizeof(Ext2DirEntryHeader);
                __builtin_memset(name_bytes, 0, max_name_bytes);
                __builtin_memcpy(name_bytes, new_name, new_name_length);
                entry->name_len = static_cast<uint8_t>(new_name_length);
                return write_block(block_number, g_block_buffer);
            }
            offset += entry->rec_len;
        }
    }

    return false;
}

bool retarget_directory_entry(Ext2Inode& directory_inode,
                              const char* name,
                              uint32_t new_inode_number,
                              uint8_t new_file_type) noexcept {
    if ((directory_inode.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    const uint32_t dir_blocks =
        (directory_inode.size + g_state.block_size - 1U) / g_state.block_size;
    for (uint32_t block_index = 0U; block_index < dir_blocks; ++block_index) {
        uint32_t block_number = 0U;
        if (!get_data_block_number(directory_inode, block_index, block_number) || block_number == 0U) {
            continue;
        }
        if (!read_block(block_number, g_block_buffer)) {
            return false;
        }

        uint32_t offset = 0U;
        while (offset + sizeof(Ext2DirEntryHeader) <= g_state.block_size) {
            auto* entry = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2DirEntryHeader) || entry->rec_len == 0U) {
                break;
            }
            if (entry->inode != 0U &&
                entry->name_len <= kMaxDirectoryEntryName &&
                name_equals(
                    reinterpret_cast<const char*>(g_block_buffer + offset + sizeof(Ext2DirEntryHeader)),
                    name,
                    entry->name_len)) {
                entry->inode = new_inode_number;
                entry->file_type = new_file_type;
                return write_block(block_number, g_block_buffer);
            }
            offset += entry->rec_len;
        }
    }

    return false;
}

bool update_directory_dotdot(const Ext2Inode& directory_inode,
                             uint32_t new_parent_inode_number) noexcept {
    if ((directory_inode.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    uint32_t block_number = 0U;
    if (!get_data_block_number(directory_inode, 0U, block_number) || block_number == 0U) {
        return false;
    }
    if (!read_block(block_number, g_block_buffer)) {
        return false;
    }

    auto* dot = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer);
    if (dot->inode == 0U || dot->name_len != 1U) {
        return false;
    }
    auto* dot_name = reinterpret_cast<const char*>(g_block_buffer + sizeof(Ext2DirEntryHeader));
    if (!name_equals(dot_name, ".", 1U)) {
        return false;
    }
    if (dot->rec_len < sizeof(Ext2DirEntryHeader) ||
        dot->rec_len + sizeof(Ext2DirEntryHeader) > g_state.block_size) {
        return false;
    }

    auto* dotdot = reinterpret_cast<Ext2DirEntryHeader*>(g_block_buffer + dot->rec_len);
    if (dotdot->inode == 0U || dotdot->name_len != 2U) {
        return false;
    }
    auto* dotdot_name =
        reinterpret_cast<const char*>(g_block_buffer + dot->rec_len + sizeof(Ext2DirEntryHeader));
    if (!name_equals(dotdot_name, "..", 2U)) {
        return false;
    }

    dotdot->inode = new_parent_inode_number;
    return write_block(block_number, g_block_buffer);
}

bool rename_node(const char* old_path, const char* new_path) noexcept {
    if (string_equals(old_path, new_path)) {
        return true;
    }

    char old_parent_path[kMaxPersistPath]{};
    char new_parent_path[kMaxPersistPath]{};
    char old_leaf[kMaxPathComponent + 1U]{};
    char new_leaf[kMaxPathComponent + 1U]{};
    if (!split_parent_path(old_path, old_parent_path, sizeof(old_parent_path), old_leaf, sizeof(old_leaf)) ||
        !split_parent_path(new_path, new_parent_path, sizeof(new_parent_path), new_leaf, sizeof(new_leaf))) {
        return false;
    }

    Ext2Inode source_parent{};
    uint32_t source_parent_inode_number = 0U;
    if (!resolve_path_with_inode_number(old_parent_path, source_parent, source_parent_inode_number) ||
        (source_parent.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    uint32_t child_inode_number = 0U;
    if (!lookup_in_directory(source_parent, old_leaf, child_inode_number) || child_inode_number == 0U) {
        return false;
    }

    Ext2Inode child_inode{};
    if (!read_inode(child_inode_number, child_inode)) {
        return false;
    }
    const bool child_is_directory = (child_inode.mode & kFileTypeMask) == kDirectoryType;

    Ext2Inode target_parent{};
    uint32_t target_parent_inode_number = 0U;
    if (!resolve_path_with_inode_number(new_parent_path, target_parent, target_parent_inode_number) ||
        (target_parent.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    uint32_t existing_target_inode = 0U;
    const bool target_exists = lookup_in_directory(target_parent, new_leaf, existing_target_inode);

    if (source_parent_inode_number == target_parent_inode_number) {
        if (target_exists) {
            // Continue into the replacement path below.
        } else {
            return rename_directory_entry_in_place(source_parent, old_leaf, new_leaf);
        }
    }

    Ext2Inode replaced_inode{};
    bool replacing_existing_file = false;
    if (target_exists) {
        if (!read_inode(existing_target_inode, replaced_inode)) {
            return false;
        }
        const bool replaced_is_directory = (replaced_inode.mode & kFileTypeMask) == kDirectoryType;
        if (child_is_directory || replaced_is_directory) {
            return false;
        }
        replacing_existing_file = true;
    }

    if (child_is_directory && path_is_at_or_below(new_parent_path, old_path)) {
        return false;
    }

    if (replacing_existing_file) {
        if (!retarget_directory_entry(target_parent,
                                      new_leaf,
                                      child_inode_number,
                                      inode_dirent_type(child_inode))) {
            return false;
        }
    } else {
        if (!add_directory_entry(target_parent,
                                 target_parent_inode_number,
                                 new_leaf,
                                 child_inode_number,
                                 inode_dirent_type(child_inode))) {
            return false;
        }
    }

    if (child_is_directory && !replacing_existing_file) {
        ++target_parent.links_count;
        if (!write_inode(target_parent_inode_number, target_parent)) {
            return false;
        }
    }

    bool removed_directory = false;
    uint32_t removed_inode_number = 0U;
    if (!remove_directory_entry(source_parent,
                                source_parent_inode_number,
                                old_leaf,
                                removed_inode_number,
                                removed_directory) ||
        removed_inode_number != child_inode_number) {
        return false;
    }
    if (removed_directory != child_is_directory) {
        removed_directory = child_is_directory;
    }

    if (removed_directory) {
        if (!update_directory_dotdot(child_inode, target_parent_inode_number)) {
            return false;
        }
        if (source_parent.links_count == 0U) {
            return false;
        }
        --source_parent.links_count;
        if (!write_inode(source_parent_inode_number, source_parent)) {
            return false;
        }
    }

    if (replacing_existing_file) {
        if (!free_inode_storage(replaced_inode, existing_target_inode)) {
            return false;
        }
        if (!free_group0_inode(existing_target_inode, false)) {
            return false;
        }
    }

    g_executable_valid = false;
    return true;
}

bool remove_node(const char* path) noexcept {
    char parent_path[kMaxPersistPath]{};
    char leaf_name[kMaxPathComponent + 1U]{};
    if (!split_parent_path(path, parent_path, sizeof(parent_path), leaf_name, sizeof(leaf_name))) {
        return false;
    }

    Ext2Inode parent{};
    uint32_t parent_inode_number = 0U;
    if (!resolve_path_with_inode_number(parent_path, parent, parent_inode_number) ||
        (parent.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }

    uint32_t child_inode_number = 0U;
    if (!lookup_in_directory(parent, leaf_name, child_inode_number) || child_inode_number == 0U) {
        return false;
    }

    Ext2Inode child{};
    if (!read_inode(child_inode_number, child)) {
        return false;
    }
    const bool directory = (child.mode & kFileTypeMask) == kDirectoryType;
    if (directory && !directory_is_empty(child)) {
        return false;
    }

    bool child_is_directory = false;
    uint32_t removed_inode_number = 0U;
    if (!remove_directory_entry(parent,
                                parent_inode_number,
                                leaf_name,
                                removed_inode_number,
                                child_is_directory) ||
        removed_inode_number != child_inode_number) {
        return false;
    }
    if (child_is_directory != directory) {
        child_is_directory = directory;
    }

    if (!free_inode_storage(child, child_inode_number)) {
        return false;
    }
    if (!free_group0_inode(child_inode_number, directory)) {
        return false;
    }

    if (directory && parent.links_count != 0U) {
        --parent.links_count;
        if (!write_inode(parent_inode_number, parent)) {
            return false;
        }
    }

    g_executable_valid = false;
    return true;
}

struct DirectoryListingContext {
    char* buffer;
    uint32_t capacity;
    uint32_t index;
};

bool append_directory_entry(const Ext2DirEntryHeader&,
                            const char* name,
                            bool is_directory,
                            void* context) noexcept {
    auto* listing = static_cast<DirectoryListingContext*>(context);
    if (listing == nullptr || name == nullptr) {
        return false;
    }
    if (name_equals(name, ".", 1U) || name_equals(name, "..", 2U)) {
        return true;
    }

    const uint32_t name_length = string_length(name);
    uint32_t required = name_length + 1U;
    if (is_directory) {
        ++required;
    }
    if (listing->index + required >= listing->capacity) {
        return false;
    }

    __builtin_memcpy(listing->buffer + listing->index, name, name_length);
    listing->index += name_length;
    if (is_directory) {
        listing->buffer[listing->index++] = '/';
    }
    listing->buffer[listing->index++] = '\n';
    listing->buffer[listing->index] = '\0';
    return true;
}

} // namespace

void probe() noexcept {
    g_state = {false, {}, {}, 0U, 0U};
    g_executable_valid = false;
    g_executable_size = 0U;
    __builtin_memset(g_executable_path, 0, sizeof(g_executable_path));

    if (storage::boot_partition() == nullptr) {
        write_key("ext2 reader");
        console::write_string("no boot partition");
        console::newline();
        return;
    }

    if (!load_superblock()) {
        write_key("ext2 reader");
        console::write_string("superblock load failed");
        console::newline();
        return;
    }

    write_key("ext2 reader");
    console::write_string("ready");
    console::newline();

    log_persist_file_probe();
}

bool register_bootfs_mount() noexcept {
    if (!g_state.valid) {
        return false;
    }

    write_key("ext2 mount");
    if (bootfs::register_mount_directory("/persist") != 0) {
        console::write_string("registration failed");
        console::newline();
        return false;
    }

    console::write_string("ready path=/persist");
    console::newline();
    return true;
}

bool query_runtime_path(const char* path, NodeInfo& info) noexcept {
    info = {false, false, false, 0U};
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }

    Ext2Inode inode{};
    if (!resolve_path(ext2_path, inode)) {
        return false;
    }

    info.exists = true;
    info.is_directory = (inode.mode & kFileTypeMask) == kDirectoryType;
    info.executable = (inode.mode & 0111U) != 0U;
    info.size = inode.size;
    return true;
}

bool query_persist_path(const char* path, NodeInfo& info) noexcept {
    if (path == nullptr || !starts_with(path, "/persist")) {
        info = {false, false, false, 0U};
        return false;
    }
    return query_runtime_path(path, info);
}

bool load_runtime_executable(const char* path,
                             const uint8_t** image,
                             uint32_t* size) noexcept {
    if (image == nullptr || size == nullptr) {
        return false;
    }
    *image = nullptr;
    *size = 0U;

    NodeInfo info{};
    if (!query_runtime_path(path, info) || info.is_directory || !info.executable ||
        info.size == 0U || info.size > sizeof(g_executable_buffer)) {
        return false;
    }

    if (g_executable_valid && string_equals(path, g_executable_path)) {
        *image = g_executable_buffer;
        *size = g_executable_size;
        return true;
    }

    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }

    Ext2Inode inode{};
    if (!resolve_path(ext2_path, inode)) {
        return false;
    }

    uint32_t bytes_read = 0U;
    if (!read_inode_range(inode, 0U, g_executable_buffer, info.size, bytes_read) ||
        bytes_read != info.size) {
        return false;
    }

    cache_executable_path(path);
    g_executable_size = bytes_read;
    g_executable_valid = true;
    *image = g_executable_buffer;
    *size = g_executable_size;
    return true;
}

bool load_persist_executable(const char* path,
                             const uint8_t** image,
                             uint32_t* size) noexcept {
    if (path == nullptr || !starts_with(path, "/persist")) {
        if (image != nullptr) {
            *image = nullptr;
        }
        if (size != nullptr) {
            *size = 0U;
        }
        return false;
    }
    return load_runtime_executable(path, image, size);
}

bool create_runtime_file(const char* path, uint16_t mode) noexcept {
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }
    return create_node(ext2_path, mode == 0U ? 0644U : mode, false);
}

bool mkdir_runtime_directory(const char* path, uint16_t mode) noexcept {
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }
    return create_node(ext2_path, mode == 0U ? kExt2RootPerms : mode, true);
}

bool rename_runtime_path(const char* old_path, const char* new_path) noexcept {
    char old_ext2_path[kMaxPersistPath]{};
    char new_ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(old_path, old_ext2_path, sizeof(old_ext2_path)) ||
        !map_runtime_path(new_path, new_ext2_path, sizeof(new_ext2_path))) {
        return false;
    }
    if (string_equals(old_ext2_path, "/") || string_equals(new_ext2_path, "/")) {
        return false;
    }
    return rename_node(old_ext2_path, new_ext2_path);
}

bool unlink_runtime_path(const char* path) noexcept {
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }
    Ext2Inode inode{};
    if (!resolve_path(ext2_path, inode) || (inode.mode & kFileTypeMask) == kDirectoryType) {
        return false;
    }
    return remove_node(ext2_path);
}

bool rmdir_runtime_directory(const char* path) noexcept {
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }
    if (string_equals(ext2_path, "/")) {
        return false;
    }
    Ext2Inode inode{};
    if (!resolve_path(ext2_path, inode) || (inode.mode & kFileTypeMask) != kDirectoryType) {
        return false;
    }
    return remove_node(ext2_path);
}

uint32_t allocated_file_capacity(const Ext2Inode& inode) noexcept {
    uint32_t capacity = 0U;
    for (uint32_t block_index = 0U; block_index < kExt2DirectBlocks; ++block_index) {
        if (inode.block[block_index] != 0U) {
            capacity += g_state.block_size;
        }
    }

    const uint32_t indirect_block = inode.block[kSingleIndirectIndex];
    if (indirect_block != 0U && read_block(indirect_block, g_indirect_block_buffer)) {
        const uint32_t entries_per_indirect_block = g_state.block_size / sizeof(uint32_t);
        const auto* entries = reinterpret_cast<const uint32_t*>(g_indirect_block_buffer);
        for (uint32_t index = 0U; index < entries_per_indirect_block; ++index) {
            if (entries[index] != 0U) {
                capacity += g_state.block_size;
            }
        }
    }
    return capacity;
}

void subtract_inode_block_usage(Ext2Inode& inode, uint32_t byte_count) noexcept {
    const uint32_t sectors = bytes_to_sectors(byte_count);
    inode.blocks = inode.blocks > sectors ? inode.blocks - sectors : 0U;
}

bool zero_inode_tail_range(const Ext2Inode& inode, uint32_t size) noexcept {
    if (size == 0U || (size % g_state.block_size) == 0U) {
        return true;
    }

    const uint32_t logical_block_index = size / g_state.block_size;
    uint32_t block_number = 0U;
    if (!get_data_block_number(inode, logical_block_index, block_number)) {
        return false;
    }
    if (block_number == 0U) {
        return true;
    }
    if (!read_block(block_number, g_block_buffer)) {
        return false;
    }

    const uint32_t offset_in_block = size % g_state.block_size;
    __builtin_memset(g_block_buffer + offset_in_block, 0, g_state.block_size - offset_in_block);
    return write_block(block_number, g_block_buffer);
}

bool trim_inode_direct_blocks(Ext2Inode& inode, uint32_t first_logical_block_to_free) noexcept {
    if (first_logical_block_to_free >= kExt2DirectBlocks) {
        return true;
    }

    for (uint32_t index = first_logical_block_to_free; index < kExt2DirectBlocks; ++index) {
        if (inode.block[index] == 0U) {
            continue;
        }
        if (!free_group0_block(inode.block[index])) {
            return false;
        }
        inode.block[index] = 0U;
        subtract_inode_block_usage(inode, g_state.block_size);
    }
    return true;
}

bool trim_inode_indirect_blocks(Ext2Inode& inode, uint32_t first_logical_block_to_free) noexcept {
    const uint32_t indirect_block = inode.block[kSingleIndirectIndex];
    if (indirect_block == 0U) {
        return true;
    }

    if (!read_block(indirect_block, g_indirect_block_buffer)) {
        return false;
    }

    auto* entries = reinterpret_cast<uint32_t*>(g_indirect_block_buffer);
    const uint32_t entries_per_indirect_block = g_state.block_size / sizeof(uint32_t);
    bool changed = false;
    for (uint32_t index = 0U; index < entries_per_indirect_block; ++index) {
        const uint32_t logical_block_index = kExt2DirectBlocks + index;
        if (logical_block_index < first_logical_block_to_free || entries[index] == 0U) {
            continue;
        }
        if (!free_group0_block(entries[index])) {
            return false;
        }
        entries[index] = 0U;
        subtract_inode_block_usage(inode, g_state.block_size);
        changed = true;
    }

    bool any_entries_remaining = false;
    for (uint32_t index = 0U; index < entries_per_indirect_block; ++index) {
        if (entries[index] != 0U) {
            any_entries_remaining = true;
            break;
        }
    }

    if (!any_entries_remaining) {
        if (!free_group0_block(indirect_block)) {
            return false;
        }
        inode.block[kSingleIndirectIndex] = 0U;
        subtract_inode_block_usage(inode, g_state.block_size);
        return true;
    }

    if (changed && !write_block(indirect_block, g_indirect_block_buffer)) {
        return false;
    }
    return true;
}

bool truncate_runtime_file(const char* path, uint32_t size) noexcept {
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return false;
    }

    Ext2Inode inode{};
    uint32_t inode_number = 0U;
    if (!resolve_path_with_inode_number(ext2_path, inode, inode_number) ||
        (inode.mode & kFileTypeMask) != kRegularFileType) {
        return false;
    }

    if (size > inode.size) {
        return false;
    }

    const uint32_t capacity = allocated_file_capacity(inode);
    if (size > capacity) {
        return false;
    }

    const uint32_t first_logical_block_to_free =
        size == 0U ? 0U : ((size + g_state.block_size - 1U) / g_state.block_size);
    if (!zero_inode_tail_range(inode, size)) {
        return false;
    }
    if (!trim_inode_indirect_blocks(inode, first_logical_block_to_free)) {
        return false;
    }
    if (!trim_inode_direct_blocks(inode, first_logical_block_to_free)) {
        return false;
    }

    inode.size = size;
    inode.dir_acl = size;
    g_executable_valid = false;
    return write_inode(inode_number, inode);
}

int read_runtime_file(const char* path,
                      uint32_t offset,
                      uint8_t* buffer,
                      uint32_t count) noexcept {
    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return -1;
    }

    Ext2Inode inode{};
    if (!resolve_path(ext2_path, inode)) {
        return -1;
    }

    uint32_t bytes_read = 0U;
    if (!read_inode_range(inode, offset, buffer, count, bytes_read)) {
        return -1;
    }
    return static_cast<int>(bytes_read);
}

int write_runtime_file(const char* path,
                       uint32_t offset,
                       const uint8_t* buffer,
                       uint32_t count) noexcept {
    if (buffer == nullptr || count == 0U) {
        return 0;
    }

    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return -1;
    }

    Ext2Inode inode{};
    uint32_t inode_number = 0U;
    if (!resolve_path_with_inode_number(ext2_path, inode, inode_number) ||
        (inode.mode & kFileTypeMask) != kRegularFileType) {
        return -1;
    }

    auto zero_fill_gap = [&](uint32_t start_offset, uint32_t end_offset) noexcept -> bool {
        if (end_offset <= start_offset) {
            return true;
        }

        uint32_t cursor = start_offset;
        while (cursor < end_offset) {
            const uint32_t block_index = cursor / g_state.block_size;
            uint32_t block_number = 0U;
            if (!ensure_inode_data_block(inode, inode_number, block_index, block_number) ||
                block_number == 0U) {
                return false;
            }

            const uint32_t offset_in_block = cursor % g_state.block_size;
            uint32_t chunk = g_state.block_size - offset_in_block;
            if (chunk > (end_offset - cursor)) {
                chunk = end_offset - cursor;
            }

            if (!read_block(block_number, g_block_buffer)) {
                return false;
            }
            __builtin_memset(g_block_buffer + offset_in_block, 0, chunk);
            if (!write_block(block_number, g_block_buffer)) {
                return false;
            }
            cursor += chunk;
        }
        return true;
    };

    if (offset > inode.size && !zero_fill_gap(inode.size, offset)) {
        return -1;
    }

    uint32_t bytes_written = 0U;
    while (bytes_written < count) {
        const uint32_t absolute_offset = offset + bytes_written;
        const uint32_t block_index = absolute_offset / g_state.block_size;
        uint32_t block_number = 0U;
        if (!ensure_inode_data_block(inode, inode_number, block_index, block_number) ||
            block_number == 0U) {
            return bytes_written == 0U ? -1 : static_cast<int>(bytes_written);
        }

        const uint32_t offset_in_block = absolute_offset % g_state.block_size;
        uint32_t chunk = g_state.block_size - offset_in_block;
        if (chunk > (count - bytes_written)) {
            chunk = count - bytes_written;
        }

        if (!read_block(block_number, g_block_buffer)) {
            return bytes_written == 0U ? -1 : static_cast<int>(bytes_written);
        }
        __builtin_memcpy(g_block_buffer + offset_in_block, buffer + bytes_written, chunk);
        if (!write_block(block_number, g_block_buffer)) {
            return bytes_written == 0U ? -1 : static_cast<int>(bytes_written);
        }
        bytes_written += chunk;
    }

    const uint32_t end_offset = offset + bytes_written;
    if (end_offset > inode.size) {
        inode.size = end_offset;
        inode.dir_acl = end_offset;
        if (!write_inode(inode_number, inode)) {
            return bytes_written == 0U ? -1 : static_cast<int>(bytes_written);
        }
    }

    g_executable_valid = false;
    return static_cast<int>(bytes_written);
}

int read_persist_file(const char* path,
                      uint32_t offset,
                      uint8_t* buffer,
                      uint32_t count) noexcept {
    if (path == nullptr || !starts_with(path, "/persist")) {
        return -1;
    }
    return read_runtime_file(path, offset, buffer, count);
}

uint32_t build_runtime_directory_listing(const char* path,
                                         char* buffer,
                                         uint32_t capacity) noexcept {
    if (buffer == nullptr || capacity == 0U) {
        return 0U;
    }
    buffer[0] = '\0';

    char ext2_path[kMaxPersistPath]{};
    if (!map_runtime_path(path, ext2_path, sizeof(ext2_path))) {
        return 0U;
    }

    Ext2Inode inode{};
    if (!resolve_path(ext2_path, inode)) {
        return 0U;
    }

    DirectoryListingContext context{buffer, capacity, 0U};
    if (!visit_directory_entries(inode, append_directory_entry, &context)) {
        return 0U;
    }
    return context.index;
}

uint32_t build_persist_directory_listing(const char* path,
                                         char* buffer,
                                         uint32_t capacity) noexcept {
    if (path == nullptr || !starts_with(path, "/persist")) {
        if (buffer != nullptr && capacity != 0U) {
            buffer[0] = '\0';
        }
        return 0U;
    }
    return build_runtime_directory_listing(path, buffer, capacity);
}

} // namespace xinim::i486::ext2_reader

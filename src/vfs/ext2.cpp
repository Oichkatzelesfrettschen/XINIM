/**
 * @file ext2.cpp
 * @brief ext2 Filesystem Driver Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/ext2.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <ctime>

// Error codes
#define EINVAL 22     // Invalid argument
#define EIO    5      // I/O error
#define ENOENT 2      // No such file or directory
#define EISDIR 21     // Is a directory
#define ENOTDIR 20    // Not a directory
#define EROFS  30     // Read-only filesystem
#define ENOSPC 28     // No space left on device
#define EEXIST 17     // File exists
#define ENOTEMPTY 39  // Directory not empty

namespace xinim::vfs {

// ============================================================================
// Ext2Filesystem Implementation
// ============================================================================

Ext2Filesystem::Ext2Filesystem()
    : BlockFilesystem(),
      block_size_(0),
      inode_size_(0),
      num_block_groups_(0),
      superblock_block_(0) {
    std::memset(&superblock_, 0, sizeof(superblock_));
}

Ext2Filesystem::~Ext2Filesystem() {
    // Base class handles unmount
}

int Ext2Filesystem::read_superblock() {
    // ext2 superblock is located at byte offset 1024
    constexpr uint64_t SUPERBLOCK_OFFSET = 1024;

    // Read the device to get actual block size first
    auto block_dev = get_block_device();
    if (!block_dev) {
        return -EIO;
    }

    size_t dev_block_size = block_dev->get_block_size();

    // Calculate which device block contains the superblock
    uint64_t dev_block = SUPERBLOCK_OFFSET / dev_block_size;
    uint64_t offset_in_block = SUPERBLOCK_OFFSET % dev_block_size;

    // Read the block containing the superblock
    std::vector<uint8_t> block_buffer(dev_block_size);
    int ret = block_dev->read_blocks(dev_block, 1, block_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to read superblock: %d", ret);
        return ret;
    }

    // Copy superblock from buffer
    std::memcpy(&superblock_, block_buffer.data() + offset_in_block, sizeof(Ext2Superblock));

    // Validate superblock
    ret = validate_superblock();
    if (ret < 0) {
        return ret;
    }

    // Calculate filesystem parameters
    block_size_ = 1024 << superblock_.s_log_block_size;

    if (superblock_.s_rev_level == Ext2Superblock::EXT2_DYNAMIC_REV) {
        inode_size_ = superblock_.s_inode_size;
    } else {
        inode_size_ = Ext2Superblock::EXT2_GOOD_OLD_INODE_SIZE;
    }

    num_block_groups_ = (superblock_.s_blocks_count + superblock_.s_blocks_per_group - 1) /
                        superblock_.s_blocks_per_group;

    // Store filesystem statistics for base class
    block_size_ = 1024 << superblock_.s_log_block_size;
    total_blocks_ = superblock_.s_blocks_count;
    free_blocks_ = superblock_.s_free_blocks_count;
    total_inodes_ = superblock_.s_inodes_count;
    free_inodes_ = superblock_.s_free_inodes_count;

    LOG_INFO("ext2: Superblock loaded successfully");
    LOG_INFO("ext2:   Block size: %u bytes", block_size_);
    LOG_INFO("ext2:   Total blocks: %lu", (unsigned long)total_blocks_);
    LOG_INFO("ext2:   Free blocks: %lu", (unsigned long)free_blocks_);
    LOG_INFO("ext2:   Total inodes: %lu", (unsigned long)total_inodes_);
    LOG_INFO("ext2:   Free inodes: %lu", (unsigned long)free_inodes_);
    LOG_INFO("ext2:   Inode size: %u bytes", inode_size_);
    LOG_INFO("ext2:   Block groups: %u", num_block_groups_);

    // Read group descriptors
    ret = read_group_descriptors();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int Ext2Filesystem::validate_superblock() {
    // Check magic number
    if (superblock_.s_magic != Ext2Superblock::EXT2_MAGIC) {
        LOG_ERROR("ext2: Invalid magic number: 0x%04X (expected 0x%04X)",
                  superblock_.s_magic, Ext2Superblock::EXT2_MAGIC);
        return -EINVAL;
    }

    // Check revision
    if (superblock_.s_rev_level > Ext2Superblock::EXT2_DYNAMIC_REV) {
        LOG_WARN("ext2: Unknown revision level: %u", superblock_.s_rev_level);
    }

    // Check filesystem state
    if (superblock_.s_state == Ext2Superblock::EXT2_ERROR_FS) {
        LOG_WARN("ext2: Filesystem has errors, mounting read-only");
    }

    // Basic sanity checks
    if (superblock_.s_blocks_count == 0 || superblock_.s_inodes_count == 0) {
        LOG_ERROR("ext2: Invalid block or inode count");
        return -EINVAL;
    }

    if (superblock_.s_blocks_per_group == 0 || superblock_.s_inodes_per_group == 0) {
        LOG_ERROR("ext2: Invalid blocks/inodes per group");
        return -EINVAL;
    }

    return 0;
}

int Ext2Filesystem::read_group_descriptors() {
    // Group descriptors start immediately after the superblock
    // If block size is 1024, superblock is in block 1, so GDT is in block 2
    // If block size is >1024, superblock is in block 0, so GDT is in block 1
    uint32_t gdt_block = (block_size_ == 1024) ? 2 : 1;

    // Calculate how many blocks the GDT spans
    size_t gdt_size = num_block_groups_ * sizeof(Ext2GroupDescriptor);
    size_t gdt_blocks = (gdt_size + block_size_ - 1) / block_size_;

    // Allocate buffer for GDT
    std::vector<uint8_t> gdt_buffer(gdt_blocks * block_size_);

    // Read GDT blocks
    int ret = read_blocks(gdt_block, gdt_blocks, gdt_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to read group descriptors: %d", ret);
        return ret;
    }

    // Parse group descriptors
    group_descriptors_.resize(num_block_groups_);
    std::memcpy(group_descriptors_.data(), gdt_buffer.data(),
                num_block_groups_ * sizeof(Ext2GroupDescriptor));

    LOG_INFO("ext2: Loaded %u block group descriptors", num_block_groups_);

    return 0;
}

int Ext2Filesystem::write_superblock() {
    // ext2 superblock is located at byte offset 1024
    constexpr uint64_t SUPERBLOCK_OFFSET = 1024;

    auto block_dev = get_block_device();
    if (!block_dev) {
        return -EIO;
    }

    size_t dev_block_size = block_dev->get_block_size();

    // Calculate which device block contains the superblock
    uint64_t dev_block = SUPERBLOCK_OFFSET / dev_block_size;
    uint64_t offset_in_block = SUPERBLOCK_OFFSET % dev_block_size;

    // Read the existing block
    std::vector<uint8_t> block_buffer(dev_block_size);
    int ret = block_dev->read_blocks(dev_block, 1, block_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to read block for superblock write: %d", ret);
        return ret;
    }

    // Update timestamps
    superblock_.s_wtime = static_cast<uint32_t>(time(nullptr));  // TODO: Use kernel time

    // Copy superblock to buffer
    std::memcpy(block_buffer.data() + offset_in_block, &superblock_, sizeof(Ext2Superblock));

    // Write the block back
    ret = block_dev->write_blocks(dev_block, 1, block_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to write superblock: %d", ret);
        return ret;
    }

    // Sync to ensure it's written
    block_dev->flush();

    return 0;
}

VNode* Ext2Filesystem::create_root_vnode() {
    // Root directory is always inode 2
    constexpr uint32_t ROOT_INODE = 2;

    try {
        Ext2Node* root = new Ext2Node(this, ROOT_INODE);
        return root;
    } catch (...) {
        LOG_ERROR("ext2: Failed to create root vnode");
        return nullptr;
    }
}

int Ext2Filesystem::flush_all() {
    int ret;

    // Write group descriptors
    ret = write_group_descriptors();
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to write group descriptors during flush: %d", ret);
        return ret;
    }

    // Write superblock
    ret = write_superblock();
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to write superblock during flush: %d", ret);
        return ret;
    }

    // Flush block device cache
    auto block_dev = get_block_device();
    if (block_dev) {
        block_dev->flush();
    }

    return 0;
}

uint32_t Ext2Filesystem::get_block_group(uint32_t inode_num) const {
    // Inode numbers start at 1
    return (inode_num - 1) / superblock_.s_inodes_per_group;
}

uint32_t Ext2Filesystem::get_group_index(uint32_t inode_num) const {
    // Inode numbers start at 1
    return (inode_num - 1) % superblock_.s_inodes_per_group;
}

uint64_t Ext2Filesystem::get_inode_offset(uint32_t inode_num) const {
    uint32_t group = get_block_group(inode_num);
    uint32_t index = get_group_index(inode_num);

    if (group >= num_block_groups_) {
        return 0;
    }

    // Get inode table block for this group
    uint32_t inode_table_block = group_descriptors_[group].bg_inode_table;

    // Calculate byte offset within the inode table
    uint64_t offset_in_table = index * inode_size_;

    // Calculate absolute byte offset
    uint64_t block_offset = offset_in_table / block_size_;
    uint64_t byte_offset = offset_in_table % block_size_;

    return (inode_table_block + block_offset) * block_size_ + byte_offset;
}

int Ext2Filesystem::read_inode(uint32_t inode_num, Ext2Inode& inode) {
    if (inode_num == 0 || inode_num > superblock_.s_inodes_count) {
        LOG_ERROR("ext2: Invalid inode number: %u", inode_num);
        return -EINVAL;
    }

    uint32_t group = get_block_group(inode_num);
    uint32_t index = get_group_index(inode_num);

    if (group >= num_block_groups_) {
        LOG_ERROR("ext2: Inode %u is in invalid group %u", inode_num, group);
        return -EINVAL;
    }

    // Get inode table block
    uint32_t inode_table_block = group_descriptors_[group].bg_inode_table;

    // Calculate which block contains this inode
    uint32_t inodes_per_block = block_size_ / inode_size_;
    uint32_t block_offset = index / inodes_per_block;
    uint32_t inode_offset = index % inodes_per_block;

    // Read the block containing the inode
    std::vector<uint8_t> block_buffer(block_size_);
    int ret = read_block(inode_table_block + block_offset, block_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to read inode %u: %d", inode_num, ret);
        return ret;
    }

    // Copy inode from buffer
    size_t inode_pos = inode_offset * inode_size_;
    std::memcpy(&inode, block_buffer.data() + inode_pos, sizeof(Ext2Inode));

    return 0;
}

int Ext2Filesystem::write_inode(uint32_t inode_num, const Ext2Inode& inode) {
    if (inode_num == 0 || inode_num > superblock_.s_inodes_count) {
        LOG_ERROR("ext2: Invalid inode number: %u", inode_num);
        return -EINVAL;
    }

    uint32_t group = get_block_group(inode_num);
    uint32_t index = get_group_index(inode_num);

    if (group >= num_block_groups_) {
        LOG_ERROR("ext2: Inode %u is in invalid group %u", inode_num, group);
        return -EINVAL;
    }

    // Get inode table block
    uint32_t inode_table_block = group_descriptors_[group].bg_inode_table;

    // Calculate which block contains this inode
    uint32_t inodes_per_block = block_size_ / inode_size_;
    uint32_t block_offset = index / inodes_per_block;
    uint32_t inode_offset = index % inodes_per_block;

    // Read the block containing the inode
    std::vector<uint8_t> block_buffer(block_size_);
    int ret = read_block(inode_table_block + block_offset, block_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to read inode block for write %u: %d", inode_num, ret);
        return ret;
    }

    // Update inode in buffer
    size_t inode_pos = inode_offset * inode_size_;
    std::memcpy(block_buffer.data() + inode_pos, &inode, sizeof(Ext2Inode));

    // Write the block back
    ret = write_block(inode_table_block + block_offset, block_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to write inode %u: %d", inode_num, ret);
        return ret;
    }

    return 0;
}

int Ext2Filesystem::read_data_block(uint32_t block_num, void* buffer) {
    if (block_num == 0) {
        // Sparse block (hole in file)
        std::memset(buffer, 0, block_size_);
        return 0;
    }

    if (block_num >= superblock_.s_blocks_count) {
        LOG_ERROR("ext2: Invalid block number: %u", block_num);
        return -EINVAL;
    }

    return read_block(block_num, buffer);
}

int Ext2Filesystem::write_data_block(uint32_t block_num, const void* buffer) {
    if (block_num == 0 || block_num >= superblock_.s_blocks_count) {
        LOG_ERROR("ext2: Invalid block number for write: %u", block_num);
        return -EINVAL;
    }

    return write_block(block_num, buffer);
}

// ============================================================================
// Bitmap Operations
// ============================================================================

int Ext2Filesystem::read_bitmap(uint32_t block_num, std::vector<uint8_t>& bitmap) {
    bitmap.resize(block_size_);
    return read_data_block(block_num, bitmap.data());
}

int Ext2Filesystem::write_bitmap(uint32_t block_num, const std::vector<uint8_t>& bitmap) {
    return write_data_block(block_num, bitmap.data());
}

int Ext2Filesystem::find_free_bit(const std::vector<uint8_t>& bitmap, uint32_t& bit_index) {
    size_t num_bits = bitmap.size() * 8;

    for (size_t i = 0; i < num_bits; i++) {
        if (!test_bit(bitmap, i)) {
            bit_index = i;
            return 0;
        }
    }

    return -ENOSPC;  // No free bit found
}

void Ext2Filesystem::set_bit(std::vector<uint8_t>& bitmap, uint32_t bit_index) {
    uint32_t byte_index = bit_index / 8;
    uint32_t bit_offset = bit_index % 8;

    if (byte_index < bitmap.size()) {
        bitmap[byte_index] |= (1 << bit_offset);
    }
}

void Ext2Filesystem::clear_bit(std::vector<uint8_t>& bitmap, uint32_t bit_index) {
    uint32_t byte_index = bit_index / 8;
    uint32_t bit_offset = bit_index % 8;

    if (byte_index < bitmap.size()) {
        bitmap[byte_index] &= ~(1 << bit_offset);
    }
}

bool Ext2Filesystem::test_bit(const std::vector<uint8_t>& bitmap, uint32_t bit_index) {
    uint32_t byte_index = bit_index / 8;
    uint32_t bit_offset = bit_index % 8;

    if (byte_index >= bitmap.size()) {
        return false;
    }

    return (bitmap[byte_index] & (1 << bit_offset)) != 0;
}

// ============================================================================
// Block Allocation
// ============================================================================

int Ext2Filesystem::allocate_block(uint32_t preferred_group, uint32_t& block_num) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    // Ensure preferred group is valid
    if (preferred_group >= num_block_groups_) {
        preferred_group = 0;
    }

    // Try preferred group first
    for (uint32_t attempt = 0; attempt < num_block_groups_; attempt++) {
        uint32_t group = (preferred_group + attempt) % num_block_groups_;

        // Check if group has free blocks
        if (group_descriptors_[group].bg_free_blocks_count == 0) {
            continue;
        }

        // Read block bitmap
        std::vector<uint8_t> bitmap;
        int ret = read_bitmap(group_descriptors_[group].bg_block_bitmap, bitmap);
        if (ret < 0) {
            continue;
        }

        // Find free block in bitmap
        uint32_t bit_index;
        ret = find_free_bit(bitmap, bit_index);
        if (ret < 0) {
            continue;  // No free block in this group
        }

        // Mark block as used
        set_bit(bitmap, bit_index);

        // Write bitmap back
        ret = write_bitmap(group_descriptors_[group].bg_block_bitmap, bitmap);
        if (ret < 0) {
            LOG_ERROR("ext2: Failed to write block bitmap for group %u", group);
            return ret;
        }

        // Calculate absolute block number
        block_num = superblock_.s_first_data_block + (group * superblock_.s_blocks_per_group) + bit_index;

        // Update group descriptor
        group_descriptors_[group].bg_free_blocks_count--;

        // Update superblock
        superblock_.s_free_blocks_count--;
        free_blocks_--;

        // Write updates to disk
        write_group_descriptors();
        write_superblock();

        LOG_DEBUG("ext2: Allocated block %u from group %u", block_num, group);
        return 0;
    }

    LOG_ERROR("ext2: No free blocks available");
    return -ENOSPC;
}

int Ext2Filesystem::free_block(uint32_t block_num) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (block_num == 0 || block_num >= superblock_.s_blocks_count) {
        return -EINVAL;
    }

    // Calculate which group this block belongs to
    uint32_t relative_block = block_num - superblock_.s_first_data_block;
    uint32_t group = relative_block / superblock_.s_blocks_per_group;
    uint32_t bit_index = relative_block % superblock_.s_blocks_per_group;

    if (group >= num_block_groups_) {
        return -EINVAL;
    }

    // Read block bitmap
    std::vector<uint8_t> bitmap;
    int ret = read_bitmap(group_descriptors_[group].bg_block_bitmap, bitmap);
    if (ret < 0) {
        return ret;
    }

    // Check if block is actually allocated
    if (!test_bit(bitmap, bit_index)) {
        LOG_WARN("ext2: Attempting to free already-free block %u", block_num);
        return 0;  // Already free
    }

    // Mark block as free
    clear_bit(bitmap, bit_index);

    // Write bitmap back
    ret = write_bitmap(group_descriptors_[group].bg_block_bitmap, bitmap);
    if (ret < 0) {
        return ret;
    }

    // Update group descriptor
    group_descriptors_[group].bg_free_blocks_count++;

    // Update superblock
    superblock_.s_free_blocks_count++;
    free_blocks_++;

    // Write updates to disk
    write_group_descriptors();
    write_superblock();

    LOG_DEBUG("ext2: Freed block %u from group %u", block_num, group);
    return 0;
}

// ============================================================================
// Inode Allocation
// ============================================================================

int Ext2Filesystem::allocate_inode(uint32_t parent_dir_inode, bool is_directory, uint32_t& inode_num) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    // Try to allocate in same group as parent directory for locality
    uint32_t preferred_group = get_block_group(parent_dir_inode);

    // Try preferred group first, then all groups
    for (uint32_t attempt = 0; attempt < num_block_groups_; attempt++) {
        uint32_t group = (preferred_group + attempt) % num_block_groups_;

        // Check if group has free inodes
        if (group_descriptors_[group].bg_free_inodes_count == 0) {
            continue;
        }

        // Read inode bitmap
        std::vector<uint8_t> bitmap;
        int ret = read_bitmap(group_descriptors_[group].bg_inode_bitmap, bitmap);
        if (ret < 0) {
            continue;
        }

        // Find free inode in bitmap
        uint32_t bit_index;
        ret = find_free_bit(bitmap, bit_index);
        if (ret < 0) {
            continue;  // No free inode in this group
        }

        // Mark inode as used
        set_bit(bitmap, bit_index);

        // Write bitmap back
        ret = write_bitmap(group_descriptors_[group].bg_inode_bitmap, bitmap);
        if (ret < 0) {
            LOG_ERROR("ext2: Failed to write inode bitmap for group %u", group);
            return ret;
        }

        // Calculate absolute inode number (inodes start at 1)
        inode_num = 1 + (group * superblock_.s_inodes_per_group) + bit_index;

        // Update group descriptor
        group_descriptors_[group].bg_free_inodes_count--;
        if (is_directory) {
            group_descriptors_[group].bg_used_dirs_count++;
        }

        // Update superblock
        superblock_.s_free_inodes_count--;
        free_inodes_--;

        // Write updates to disk
        write_group_descriptors();
        write_superblock();

        LOG_DEBUG("ext2: Allocated inode %u from group %u", inode_num, group);
        return 0;
    }

    LOG_ERROR("ext2: No free inodes available");
    return -ENOSPC;
}

int Ext2Filesystem::free_inode(uint32_t inode_num) {
    std::lock_guard<std::mutex> lock(fs_mutex_);

    if (inode_num == 0 || inode_num > superblock_.s_inodes_count) {
        return -EINVAL;
    }

    // Calculate which group this inode belongs to
    uint32_t group = get_block_group(inode_num);
    uint32_t bit_index = get_group_index(inode_num);

    if (group >= num_block_groups_) {
        return -EINVAL;
    }

    // Read the inode to check if it's a directory
    Ext2Inode inode;
    int ret = read_inode(inode_num, inode);
    bool is_directory = (ret == 0) && ((inode.i_mode & Ext2Inode::EXT2_S_IFMT) == Ext2Inode::EXT2_S_IFDIR);

    // Read inode bitmap
    std::vector<uint8_t> bitmap;
    ret = read_bitmap(group_descriptors_[group].bg_inode_bitmap, bitmap);
    if (ret < 0) {
        return ret;
    }

    // Check if inode is actually allocated
    if (!test_bit(bitmap, bit_index)) {
        LOG_WARN("ext2: Attempting to free already-free inode %u", inode_num);
        return 0;  // Already free
    }

    // Mark inode as free
    clear_bit(bitmap, bit_index);

    // Write bitmap back
    ret = write_bitmap(group_descriptors_[group].bg_inode_bitmap, bitmap);
    if (ret < 0) {
        return ret;
    }

    // Update group descriptor
    group_descriptors_[group].bg_free_inodes_count++;
    if (is_directory) {
        if (group_descriptors_[group].bg_used_dirs_count > 0) {
            group_descriptors_[group].bg_used_dirs_count--;
        }
    }

    // Update superblock
    superblock_.s_free_inodes_count++;
    free_inodes_++;

    // Write updates to disk
    write_group_descriptors();
    write_superblock();

    LOG_DEBUG("ext2: Freed inode %u from group %u", inode_num, group);
    return 0;
}

// ============================================================================
// Group Descriptor and Superblock Write Operations
// ============================================================================

int Ext2Filesystem::write_group_descriptors() {
    // Group descriptors start immediately after the superblock
    uint32_t gdt_block = (block_size_ == 1024) ? 2 : 1;

    // Calculate how many blocks the GDT spans
    size_t gdt_size = num_block_groups_ * sizeof(Ext2GroupDescriptor);
    size_t gdt_blocks = (gdt_size + block_size_ - 1) / block_size_;

    // Allocate buffer for GDT
    std::vector<uint8_t> gdt_buffer(gdt_blocks * block_size_, 0);

    // Copy group descriptors to buffer
    std::memcpy(gdt_buffer.data(), group_descriptors_.data(),
                num_block_groups_ * sizeof(Ext2GroupDescriptor));

    // Write GDT blocks
    int ret = write_blocks(gdt_block, gdt_blocks, gdt_buffer.data());
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to write group descriptors: %d", ret);
        return ret;
    }

    return 0;
}

// ============================================================================
// Ext2Node Implementation
// ============================================================================

Ext2Node::Ext2Node(Ext2Filesystem* fs, uint32_t inode_num)
    : ext2fs_(fs),
      inode_num_(inode_num),
      ref_count_(1),
      dirty_(false) {

    fs_ = fs;
    inode_number_ = inode_num;
    std::memset(&inode_, 0, sizeof(inode_));

    // Load inode from disk
    int ret = load_inode();
    if (ret < 0) {
        LOG_ERROR("ext2: Failed to load inode %u: %d", inode_num, ret);
        throw std::runtime_error("Failed to load inode");
    }
}

Ext2Node::~Ext2Node() {
    if (dirty_) {
        save_inode();
    }
}

int Ext2Node::load_inode() {
    return ext2fs_->read_inode(inode_num_, inode_);
}

int Ext2Node::save_inode() {
    if (!dirty_) {
        return 0;
    }

    int ret = ext2fs_->write_inode(inode_num_, inode_);
    if (ret == 0) {
        dirty_ = false;
    }
    return ret;
}

FileType Ext2Node::inode_type_to_file_type() const {
    uint16_t mode = inode_.i_mode & Ext2Inode::EXT2_S_IFMT;

    switch (mode) {
        case Ext2Inode::EXT2_S_IFREG:  return FileType::REGULAR;
        case Ext2Inode::EXT2_S_IFDIR:  return FileType::DIRECTORY;
        case Ext2Inode::EXT2_S_IFLNK:  return FileType::SYMLINK;
        case Ext2Inode::EXT2_S_IFBLK:  return FileType::BLOCK_DEVICE;
        case Ext2Inode::EXT2_S_IFCHR:  return FileType::CHAR_DEVICE;
        case Ext2Inode::EXT2_S_IFIFO:  return FileType::FIFO;
        case Ext2Inode::EXT2_S_IFSOCK: return FileType::SOCKET;
        default: return FileType::UNKNOWN;
    }
}

int Ext2Node::get_data_block(uint32_t block_idx, uint32_t& block_num) {
    uint32_t blocks_per_indirect = ext2fs_->get_block_size() / sizeof(uint32_t);

    // Direct blocks (0-11)
    if (block_idx < Ext2Inode::EXT2_NDIR_BLOCKS) {
        block_num = inode_.i_block[block_idx];
        return 0;
    }

    block_idx -= Ext2Inode::EXT2_NDIR_BLOCKS;

    // Single indirect block (12)
    if (block_idx < blocks_per_indirect) {
        if (inode_.i_block[Ext2Inode::EXT2_IND_BLOCK] == 0) {
            block_num = 0;  // Sparse
            return 0;
        }

        // Read indirect block
        std::vector<uint32_t> indirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                          indirect.data());
        if (ret < 0) {
            return ret;
        }

        block_num = indirect[block_idx];
        return 0;
    }

    block_idx -= blocks_per_indirect;

    // Double indirect block (13)
    if (block_idx < blocks_per_indirect * blocks_per_indirect) {
        if (inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK] == 0) {
            block_num = 0;  // Sparse
            return 0;
        }

        // Read double indirect block
        std::vector<uint32_t> dindirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                          dindirect.data());
        if (ret < 0) {
            return ret;
        }

        uint32_t indirect_idx = block_idx / blocks_per_indirect;
        uint32_t block_offset = block_idx % blocks_per_indirect;

        if (dindirect[indirect_idx] == 0) {
            block_num = 0;  // Sparse
            return 0;
        }

        // Read indirect block
        std::vector<uint32_t> indirect(blocks_per_indirect);
        ret = ext2fs_->read_data_block(dindirect[indirect_idx], indirect.data());
        if (ret < 0) {
            return ret;
        }

        block_num = indirect[block_offset];
        return 0;
    }

    block_idx -= blocks_per_indirect * blocks_per_indirect;

    // Triple indirect block (14)
    if (inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK] == 0) {
        block_num = 0;  // Sparse
        return 0;
    }

    // Read triple indirect block
    std::vector<uint32_t> tindirect(blocks_per_indirect);
    int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                      tindirect.data());
    if (ret < 0) {
        return ret;
    }

    uint32_t dindirect_idx = block_idx / (blocks_per_indirect * blocks_per_indirect);
    uint32_t remaining = block_idx % (blocks_per_indirect * blocks_per_indirect);
    uint32_t indirect_idx = remaining / blocks_per_indirect;
    uint32_t block_offset = remaining % blocks_per_indirect;

    if (tindirect[dindirect_idx] == 0) {
        block_num = 0;  // Sparse
        return 0;
    }

    // Read double indirect block
    std::vector<uint32_t> dindirect(blocks_per_indirect);
    ret = ext2fs_->read_data_block(tindirect[dindirect_idx], dindirect.data());
    if (ret < 0) {
        return ret;
    }

    if (dindirect[indirect_idx] == 0) {
        block_num = 0;  // Sparse
        return 0;
    }

    // Read indirect block
    std::vector<uint32_t> indirect(blocks_per_indirect);
    ret = ext2fs_->read_data_block(dindirect[indirect_idx], indirect.data());
    if (ret < 0) {
        return ret;
    }

    block_num = indirect[block_offset];
    return 0;
}

int Ext2Node::allocate_data_block(uint32_t block_idx, uint32_t& block_num) {
    uint32_t blocks_per_indirect = ext2fs_->get_block_size() / sizeof(uint32_t);
    uint32_t group = ext2fs_->get_block_group(inode_num_);

    // Direct blocks (0-11)
    if (block_idx < Ext2Inode::EXT2_NDIR_BLOCKS) {
        if (inode_.i_block[block_idx] != 0) {
            block_num = inode_.i_block[block_idx];
            return 0;  // Already allocated
        }

        // Allocate new block
        int ret = ext2fs_->allocate_block(group, block_num);
        if (ret < 0) {
            return ret;
        }

        inode_.i_block[block_idx] = block_num;
        dirty_ = true;
        return 0;
    }

    block_idx -= Ext2Inode::EXT2_NDIR_BLOCKS;

    // Single indirect block (12)
    if (block_idx < blocks_per_indirect) {
        // Allocate indirect block if needed
        if (inode_.i_block[Ext2Inode::EXT2_IND_BLOCK] == 0) {
            uint32_t indirect_block;
            int ret = ext2fs_->allocate_block(group, indirect_block);
            if (ret < 0) {
                return ret;
            }

            inode_.i_block[Ext2Inode::EXT2_IND_BLOCK] = indirect_block;

            // Zero the indirect block
            std::vector<uint8_t> zero_buffer(ext2fs_->get_block_size(), 0);
            ext2fs_->write_data_block(indirect_block, zero_buffer.data());

            dirty_ = true;
        }

        // Read indirect block
        std::vector<uint32_t> indirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                          indirect.data());
        if (ret < 0) {
            return ret;
        }

        // Check if block already allocated
        if (indirect[block_idx] != 0) {
            block_num = indirect[block_idx];
            return 0;
        }

        // Allocate new data block
        ret = ext2fs_->allocate_block(group, block_num);
        if (ret < 0) {
            return ret;
        }

        indirect[block_idx] = block_num;

        // Write indirect block back
        ret = ext2fs_->write_data_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                       indirect.data());
        if (ret < 0) {
            ext2fs_->free_block(block_num);  // Cleanup
            return ret;
        }

        return 0;
    }

    block_idx -= blocks_per_indirect;

    // Double indirect block (13)
    if (block_idx < blocks_per_indirect * blocks_per_indirect) {
        // Allocate double indirect block if needed
        if (inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK] == 0) {
            uint32_t dindirect_block;
            int ret = ext2fs_->allocate_block(group, dindirect_block);
            if (ret < 0) {
                return ret;
            }

            inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK] = dindirect_block;

            // Zero the double indirect block
            std::vector<uint8_t> zero_buffer(ext2fs_->get_block_size(), 0);
            ext2fs_->write_data_block(dindirect_block, zero_buffer.data());

            dirty_ = true;
        }

        // Read double indirect block
        std::vector<uint32_t> dindirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                          dindirect.data());
        if (ret < 0) {
            return ret;
        }

        uint32_t indirect_idx = block_idx / blocks_per_indirect;
        uint32_t block_offset = block_idx % blocks_per_indirect;

        // Allocate indirect block if needed
        if (dindirect[indirect_idx] == 0) {
            uint32_t indirect_block;
            ret = ext2fs_->allocate_block(group, indirect_block);
            if (ret < 0) {
                return ret;
            }

            dindirect[indirect_idx] = indirect_block;

            // Zero the indirect block
            std::vector<uint8_t> zero_buffer(ext2fs_->get_block_size(), 0);
            ext2fs_->write_data_block(indirect_block, zero_buffer.data());

            // Write double indirect block back
            ret = ext2fs_->write_data_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                           dindirect.data());
            if (ret < 0) {
                ext2fs_->free_block(indirect_block);  // Cleanup
                return ret;
            }
        }

        // Read indirect block
        std::vector<uint32_t> indirect(blocks_per_indirect);
        ret = ext2fs_->read_data_block(dindirect[indirect_idx], indirect.data());
        if (ret < 0) {
            return ret;
        }

        // Check if block already allocated
        if (indirect[block_offset] != 0) {
            block_num = indirect[block_offset];
            return 0;
        }

        // Allocate new data block
        ret = ext2fs_->allocate_block(group, block_num);
        if (ret < 0) {
            return ret;
        }

        indirect[block_offset] = block_num;

        // Write indirect block back
        ret = ext2fs_->write_data_block(dindirect[indirect_idx], indirect.data());
        if (ret < 0) {
            ext2fs_->free_block(block_num);  // Cleanup
            return ret;
        }

        return 0;
    }

    block_idx -= blocks_per_indirect * blocks_per_indirect;

    // Triple indirect block (14)
    // Allocate triple indirect block if needed
    if (inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK] == 0) {
        uint32_t tindirect_block;
        int ret = ext2fs_->allocate_block(group, tindirect_block);
        if (ret < 0) {
            return ret;
        }

        inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK] = tindirect_block;

        // Zero the triple indirect block
        std::vector<uint8_t> zero_buffer(ext2fs_->get_block_size(), 0);
        ext2fs_->write_data_block(tindirect_block, zero_buffer.data());

        dirty_ = true;
    }

    // Read triple indirect block
    std::vector<uint32_t> tindirect(blocks_per_indirect);
    int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                      tindirect.data());
    if (ret < 0) {
        return ret;
    }

    uint32_t dindirect_idx = block_idx / (blocks_per_indirect * blocks_per_indirect);
    uint32_t remaining = block_idx % (blocks_per_indirect * blocks_per_indirect);
    uint32_t indirect_idx = remaining / blocks_per_indirect;
    uint32_t block_offset = remaining % blocks_per_indirect;

    // Allocate double indirect block if needed
    if (tindirect[dindirect_idx] == 0) {
        uint32_t dindirect_block;
        ret = ext2fs_->allocate_block(group, dindirect_block);
        if (ret < 0) {
            return ret;
        }

        tindirect[dindirect_idx] = dindirect_block;

        // Zero the double indirect block
        std::vector<uint8_t> zero_buffer(ext2fs_->get_block_size(), 0);
        ext2fs_->write_data_block(dindirect_block, zero_buffer.data());

        // Write triple indirect block back
        ret = ext2fs_->write_data_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                       tindirect.data());
        if (ret < 0) {
            ext2fs_->free_block(dindirect_block);  // Cleanup
            return ret;
        }
    }

    // Read double indirect block
    std::vector<uint32_t> dindirect(blocks_per_indirect);
    ret = ext2fs_->read_data_block(tindirect[dindirect_idx], dindirect.data());
    if (ret < 0) {
        return ret;
    }

    // Allocate indirect block if needed
    if (dindirect[indirect_idx] == 0) {
        uint32_t indirect_block;
        ret = ext2fs_->allocate_block(group, indirect_block);
        if (ret < 0) {
            return ret;
        }

        dindirect[indirect_idx] = indirect_block;

        // Zero the indirect block
        std::vector<uint8_t> zero_buffer(ext2fs_->get_block_size(), 0);
        ext2fs_->write_data_block(indirect_block, zero_buffer.data());

        // Write double indirect block back
        ret = ext2fs_->write_data_block(tindirect[dindirect_idx], dindirect.data());
        if (ret < 0) {
            ext2fs_->free_block(indirect_block);  // Cleanup
            return ret;
        }
    }

    // Read indirect block
    std::vector<uint32_t> indirect(blocks_per_indirect);
    ret = ext2fs_->read_data_block(dindirect[indirect_idx], indirect.data());
    if (ret < 0) {
        return ret;
    }

    // Check if block already allocated
    if (indirect[block_offset] != 0) {
        block_num = indirect[block_offset];
        return 0;
    }

    // Allocate new data block
    ret = ext2fs_->allocate_block(group, block_num);
    if (ret < 0) {
        return ret;
    }

    indirect[block_offset] = block_num;

    // Write indirect block back
    ret = ext2fs_->write_data_block(dindirect[indirect_idx], indirect.data());
    if (ret < 0) {
        ext2fs_->free_block(block_num);  // Cleanup
        return ret;
    }

    return 0;
}

int Ext2Node::free_data_blocks(uint32_t start_block, uint32_t count) {
    if (count == 0) {
        return 0;
    }

    uint32_t blocks_per_indirect = ext2fs_->get_block_size() / sizeof(uint32_t);
    uint32_t blocks_freed = 0;

    // Free blocks starting from start_block
    for (uint32_t block_idx = start_block; block_idx < start_block + count && blocks_freed < count; block_idx++) {
        uint32_t block_num = 0;
        int ret = get_data_block(block_idx, block_num);
        if (ret < 0 || block_num == 0) {
            continue;  // Skip sparse blocks or errors
        }

        // Free the data block
        ret = ext2fs_->free_block(block_num);
        if (ret < 0) {
            LOG_WARN("ext2: Failed to free block %u: %d", block_num, ret);
        } else {
            blocks_freed++;
        }

        // Clear the block pointer
        if (block_idx < Ext2Inode::EXT2_NDIR_BLOCKS) {
            // Direct block
            inode_.i_block[block_idx] = 0;
            dirty_ = true;
        } else if (block_idx < Ext2Inode::EXT2_NDIR_BLOCKS + blocks_per_indirect) {
            // Single indirect block
            uint32_t indirect_idx = block_idx - Ext2Inode::EXT2_NDIR_BLOCKS;

            if (inode_.i_block[Ext2Inode::EXT2_IND_BLOCK] != 0) {
                // Read indirect block
                std::vector<uint32_t> indirect(blocks_per_indirect);
                ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                              indirect.data());
                if (ret == 0) {
                    indirect[indirect_idx] = 0;

                    // Write back indirect block
                    ext2fs_->write_data_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                            indirect.data());
                }
            }
        } else if (block_idx < Ext2Inode::EXT2_NDIR_BLOCKS + blocks_per_indirect +
                                blocks_per_indirect * blocks_per_indirect) {
            // Double indirect block
            uint32_t dind_base = Ext2Inode::EXT2_NDIR_BLOCKS + blocks_per_indirect;
            uint32_t dind_idx = block_idx - dind_base;
            uint32_t indirect_idx = dind_idx / blocks_per_indirect;
            uint32_t block_offset = dind_idx % blocks_per_indirect;

            if (inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK] != 0) {
                // Read double indirect block
                std::vector<uint32_t> dindirect(blocks_per_indirect);
                ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                              dindirect.data());
                if (ret == 0 && dindirect[indirect_idx] != 0) {
                    // Read indirect block
                    std::vector<uint32_t> indirect(blocks_per_indirect);
                    ret = ext2fs_->read_data_block(dindirect[indirect_idx], indirect.data());
                    if (ret == 0) {
                        indirect[block_offset] = 0;

                        // Write back indirect block
                        ext2fs_->write_data_block(dindirect[indirect_idx], indirect.data());
                    }
                }
            }
        } else {
            // Triple indirect block
            uint32_t tind_base = Ext2Inode::EXT2_NDIR_BLOCKS + blocks_per_indirect +
                                  blocks_per_indirect * blocks_per_indirect;
            uint32_t tind_idx = block_idx - tind_base;
            uint32_t dindirect_idx = tind_idx / (blocks_per_indirect * blocks_per_indirect);
            uint32_t remaining = tind_idx % (blocks_per_indirect * blocks_per_indirect);
            uint32_t indirect_idx = remaining / blocks_per_indirect;
            uint32_t block_offset = remaining % blocks_per_indirect;

            if (inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK] != 0) {
                // Read triple indirect block
                std::vector<uint32_t> tindirect(blocks_per_indirect);
                ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                              tindirect.data());
                if (ret == 0 && tindirect[dindirect_idx] != 0) {
                    // Read double indirect block
                    std::vector<uint32_t> dindirect(blocks_per_indirect);
                    ret = ext2fs_->read_data_block(tindirect[dindirect_idx], dindirect.data());
                    if (ret == 0 && dindirect[indirect_idx] != 0) {
                        // Read indirect block
                        std::vector<uint32_t> indirect(blocks_per_indirect);
                        ret = ext2fs_->read_data_block(dindirect[indirect_idx], indirect.data());
                        if (ret == 0) {
                            indirect[block_offset] = 0;

                            // Write back indirect block
                            ext2fs_->write_data_block(dindirect[indirect_idx], indirect.data());
                        }
                    }
                }
            }
        }
    }

    // Clean up empty indirect blocks after freeing data blocks
    // This is done bottom-up: indirect -> double indirect -> triple indirect

    // Free single indirect block if all entries are zero
    if (inode_.i_block[Ext2Inode::EXT2_IND_BLOCK] != 0) {
        std::vector<uint32_t> indirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                          indirect.data());
        if (ret == 0) {
            bool all_zero = true;
            for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                if (indirect[i] != 0) {
                    all_zero = false;
                    break;
                }
            }

            if (all_zero) {
                ext2fs_->free_block(inode_.i_block[Ext2Inode::EXT2_IND_BLOCK]);
                inode_.i_block[Ext2Inode::EXT2_IND_BLOCK] = 0;
                dirty_ = true;
            }
        }
    }

    // Free double indirect block and its indirect blocks if all entries are zero
    if (inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK] != 0) {
        std::vector<uint32_t> dindirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                          dindirect.data());
        if (ret == 0) {
            // First, free any empty indirect blocks
            for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                if (dindirect[i] != 0) {
                    std::vector<uint32_t> indirect(blocks_per_indirect);
                    ret = ext2fs_->read_data_block(dindirect[i], indirect.data());
                    if (ret == 0) {
                        bool all_zero = true;
                        for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                            if (indirect[j] != 0) {
                                all_zero = false;
                                break;
                            }
                        }

                        if (all_zero) {
                            ext2fs_->free_block(dindirect[i]);
                            dindirect[i] = 0;
                        }
                    }
                }
            }

            // Write back updated double indirect block
            ext2fs_->write_data_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK], dindirect.data());

            // Check if double indirect block is now empty
            bool all_zero = true;
            for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                if (dindirect[i] != 0) {
                    all_zero = false;
                    break;
                }
            }

            if (all_zero) {
                ext2fs_->free_block(inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK]);
                inode_.i_block[Ext2Inode::EXT2_DIND_BLOCK] = 0;
                dirty_ = true;
            }
        }
    }

    // Free triple indirect block and its double/indirect blocks if all entries are zero
    if (inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK] != 0) {
        std::vector<uint32_t> tindirect(blocks_per_indirect);
        int ret = ext2fs_->read_data_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                          tindirect.data());
        if (ret == 0) {
            // First, free any empty double indirect and indirect blocks
            for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                if (tindirect[i] != 0) {
                    std::vector<uint32_t> dindirect(blocks_per_indirect);
                    ret = ext2fs_->read_data_block(tindirect[i], dindirect.data());
                    if (ret == 0) {
                        // Free empty indirect blocks
                        for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                            if (dindirect[j] != 0) {
                                std::vector<uint32_t> indirect(blocks_per_indirect);
                                ret = ext2fs_->read_data_block(dindirect[j], indirect.data());
                                if (ret == 0) {
                                    bool all_zero = true;
                                    for (uint32_t k = 0; k < blocks_per_indirect; k++) {
                                        if (indirect[k] != 0) {
                                            all_zero = false;
                                            break;
                                        }
                                    }

                                    if (all_zero) {
                                        ext2fs_->free_block(dindirect[j]);
                                        dindirect[j] = 0;
                                    }
                                }
                            }
                        }

                        // Write back updated double indirect block
                        ext2fs_->write_data_block(tindirect[i], dindirect.data());

                        // Check if double indirect block is now empty
                        bool all_zero = true;
                        for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                            if (dindirect[j] != 0) {
                                all_zero = false;
                                break;
                            }
                        }

                        if (all_zero) {
                            ext2fs_->free_block(tindirect[i]);
                            tindirect[i] = 0;
                        }
                    }
                }
            }

            // Write back updated triple indirect block
            ext2fs_->write_data_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK], tindirect.data());

            // Check if triple indirect block is now empty
            bool all_zero = true;
            for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                if (tindirect[i] != 0) {
                    all_zero = false;
                    break;
                }
            }

            if (all_zero) {
                ext2fs_->free_block(inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK]);
                inode_.i_block[Ext2Inode::EXT2_TIND_BLOCK] = 0;
                dirty_ = true;
            }
        }
    }

    return blocks_freed;
}

int Ext2Node::read(void* buffer, size_t size, uint64_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this is a regular file
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFREG) {
        return -EISDIR;
    }

    // Check bounds
    if (offset >= inode_.i_size) {
        return 0;  // EOF
    }

    // Adjust size to not read past EOF
    if (offset + size > inode_.i_size) {
        size = inode_.i_size - offset;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    size_t bytes_read = 0;
    uint8_t* dest = static_cast<uint8_t*>(buffer);

    while (bytes_read < size) {
        // Calculate block index and offset within block
        uint32_t block_idx = (offset + bytes_read) / block_size;
        uint32_t block_offset = (offset + bytes_read) % block_size;
        size_t bytes_to_read = std::min(size - bytes_read, (size_t)(block_size - block_offset));

        // Get physical block number
        uint32_t block_num = 0;
        int ret = get_data_block(block_idx, block_num);
        if (ret < 0) {
            return ret;
        }

        // Read block
        std::vector<uint8_t> block_buffer(block_size);
        ret = ext2fs_->read_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            return ret;
        }

        // Copy data
        std::memcpy(dest + bytes_read, block_buffer.data() + block_offset, bytes_to_read);
        bytes_read += bytes_to_read;
    }

    // Update access time
    // TODO: Update atime when write support is added

    return bytes_read;
}

std::vector<std::string> Ext2Node::readdir() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> entries;

    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return entries;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    uint32_t dir_size = inode_.i_size;
    uint32_t offset = 0;

    while (offset < dir_size) {
        // Calculate which block
        uint32_t block_idx = offset / block_size;
        uint32_t block_offset = offset % block_size;

        // Get block number
        uint32_t block_num = 0;
        int ret = get_data_block(block_idx, block_num);
        if (ret < 0) {
            break;
        }

        // Read block
        std::vector<uint8_t> block_buffer(block_size);
        ret = ext2fs_->read_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            break;
        }

        // Parse directory entries in this block
        while (block_offset < block_size && offset < dir_size) {
            Ext2DirEntry* entry = reinterpret_cast<Ext2DirEntry*>(
                block_buffer.data() + block_offset);

            if (entry->rec_len == 0) {
                break;  // Invalid entry
            }

            // Add non-empty entries
            if (entry->inode != 0 && entry->name_len > 0) {
                std::string name(entry->name, entry->name_len);
                entries.push_back(name);
            }

            offset += entry->rec_len;
            block_offset += entry->rec_len;
        }
    }

    return entries;
}

VNode* Ext2Node::lookup(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return nullptr;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    uint32_t dir_size = inode_.i_size;
    uint32_t offset = 0;

    while (offset < dir_size) {
        // Calculate which block
        uint32_t block_idx = offset / block_size;
        uint32_t block_offset = offset % block_size;

        // Get block number
        uint32_t block_num = 0;
        int ret = get_data_block(block_idx, block_num);
        if (ret < 0) {
            return nullptr;
        }

        // Read block
        std::vector<uint8_t> block_buffer(block_size);
        ret = ext2fs_->read_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            return nullptr;
        }

        // Search directory entries in this block
        while (block_offset < block_size && offset < dir_size) {
            Ext2DirEntry* entry = reinterpret_cast<Ext2DirEntry*>(
                block_buffer.data() + block_offset);

            if (entry->rec_len == 0) {
                break;  // Invalid entry
            }

            // Check if this is the entry we're looking for
            if (entry->inode != 0 && entry->name_len > 0) {
                std::string entry_name(entry->name, entry->name_len);
                if (entry_name == name) {
                    // Found it! Create VNode for this inode
                    try {
                        return new Ext2Node(ext2fs_, entry->inode);
                    } catch (...) {
                        return nullptr;
                    }
                }
            }

            offset += entry->rec_len;
            block_offset += entry->rec_len;
        }
    }

    return nullptr;
}

FileAttributes Ext2Node::get_attributes() {
    std::lock_guard<std::mutex> lock(mutex_);

    FileAttributes attrs;
    attrs.inode_number = inode_num_;
    attrs.type = inode_type_to_file_type();
    attrs.permissions.mode = inode_.i_mode & 0x0FFF;  // Lower 12 bits
    attrs.uid = inode_.i_uid;
    attrs.gid = inode_.i_gid;
    attrs.size = inode_.i_size;
    attrs.blocks = inode_.i_blocks;
    attrs.block_size = 512;  // ext2 counts 512-byte blocks
    attrs.atime = inode_.i_atime;
    attrs.mtime = inode_.i_mtime;
    attrs.ctime = inode_.i_ctime;
    attrs.nlinks = inode_.i_links_count;

    return attrs;
}

int Ext2Node::set_attributes(const FileAttributes& attrs) {
    // TODO: Implement when adding write support
    return -EROFS;
}

void Ext2Node::ref() {
    std::lock_guard<std::mutex> lock(mutex_);
    ref_count_++;
}

void Ext2Node::unref() {
    std::lock_guard<std::mutex> lock(mutex_);
    ref_count_--;
    if (ref_count_ <= 0) {
        delete this;
    }
}

// ============================================================================
// Directory Entry Helpers
// ============================================================================

uint16_t Ext2Node::calculate_rec_len(uint8_t name_len) const {
    // Directory entry: inode(4) + rec_len(2) + name_len(1) + file_type(1) + name(variable)
    // Must be aligned to 4-byte boundary
    uint16_t min_len = 8 + name_len;  // 8 bytes for fixed fields + name
    return ((min_len + 3) / 4) * 4;    // Round up to 4-byte boundary
}

int Ext2Node::add_directory_entry(const std::string& name, uint32_t new_inode_num, uint8_t file_type) {
    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    if (name.length() > Ext2DirEntry::EXT2_NAME_LEN) {
        return -EINVAL;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    uint16_t required_len = calculate_rec_len(name.length());
    uint32_t dir_blocks = (inode_.i_size + block_size - 1) / block_size;

    // Try to find space in existing blocks
    for (uint32_t block_idx = 0; block_idx < dir_blocks; block_idx++) {
        uint32_t block_num;
        int ret = get_data_block(block_idx, block_num);
        if (ret < 0) {
            continue;
        }

        // Read directory block
        std::vector<uint8_t> block_buffer(block_size);
        ret = ext2fs_->read_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            continue;
        }

        // Scan for space in this block
        uint32_t offset = 0;
        while (offset < block_size) {
            Ext2DirEntry* entry = reinterpret_cast<Ext2DirEntry*>(block_buffer.data() + offset);

            if (entry->rec_len == 0) {
                break;  // Invalid entry
            }

            // Check if this is the last entry in the block
            uint32_t next_offset = offset + entry->rec_len;
            if (next_offset >= block_size || next_offset + 8 > block_size) {
                // This is the last entry - check if we can split it
                uint16_t actual_len = calculate_rec_len(entry->name_len);
                uint16_t available = entry->rec_len - actual_len;

                if (available >= required_len) {
                    // Split this entry
                    entry->rec_len = actual_len;

                    // Create new entry after this one
                    uint32_t new_offset = offset + actual_len;
                    Ext2DirEntry* new_entry = reinterpret_cast<Ext2DirEntry*>(
                        block_buffer.data() + new_offset);

                    new_entry->inode = new_inode_num;
                    new_entry->rec_len = available;
                    new_entry->name_len = name.length();
                    new_entry->file_type = file_type;
                    std::memcpy(new_entry->name, name.c_str(), name.length());

                    // Write block back
                    ret = ext2fs_->write_data_block(block_num, block_buffer.data());
                    if (ret < 0) {
                        return ret;
                    }

                    // Update directory times
                    update_times(false, true, true);
                    dirty_ = true;

                    return 0;
                }
                break;
            }

            offset = next_offset;
        }
    }

    // No space in existing blocks - allocate new block
    uint32_t new_block_num;
    int ret = allocate_data_block(dir_blocks, new_block_num);
    if (ret < 0) {
        return ret;
    }

    // Create new directory block with single entry
    std::vector<uint8_t> block_buffer(block_size, 0);
    Ext2DirEntry* entry = reinterpret_cast<Ext2DirEntry*>(block_buffer.data());

    entry->inode = new_inode_num;
    entry->rec_len = block_size;  // Takes entire block
    entry->name_len = name.length();
    entry->file_type = file_type;
    std::memcpy(entry->name, name.c_str(), name.length());

    // Write new block
    ret = ext2fs_->write_data_block(new_block_num, block_buffer.data());
    if (ret < 0) {
        return ret;
    }

    // Update directory size
    inode_.i_size += block_size;
    update_times(false, true, true);
    dirty_ = true;

    return 0;
}

int Ext2Node::remove_directory_entry(const std::string& name) {
    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    uint32_t dir_blocks = (inode_.i_size + block_size - 1) / block_size;

    // Scan directory blocks
    for (uint32_t block_idx = 0; block_idx < dir_blocks; block_idx++) {
        uint32_t block_num;
        int ret = get_data_block(block_idx, block_num);
        if (ret < 0) {
            continue;
        }

        // Read directory block
        std::vector<uint8_t> block_buffer(block_size);
        ret = ext2fs_->read_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            continue;
        }

        // Scan entries in this block
        uint32_t offset = 0;
        uint32_t prev_offset = 0;
        Ext2DirEntry* prev_entry = nullptr;

        while (offset < block_size) {
            Ext2DirEntry* entry = reinterpret_cast<Ext2DirEntry*>(block_buffer.data() + offset);

            if (entry->rec_len == 0) {
                break;
            }

            // Check if this is the entry to remove
            if (entry->inode != 0 && entry->name_len == name.length() &&
                std::memcmp(entry->name, name.c_str(), name.length()) == 0) {

                // Found it! Remove by extending previous entry's rec_len
                if (prev_entry) {
                    prev_entry->rec_len += entry->rec_len;
                } else {
                    // First entry - just zero the inode
                    entry->inode = 0;
                }

                // Write block back
                ret = ext2fs_->write_data_block(block_num, block_buffer.data());
                if (ret < 0) {
                    return ret;
                }

                // Update directory times
                update_times(false, true, true);
                dirty_ = true;

                return 0;
            }

            prev_entry = entry;
            prev_offset = offset;
            offset += entry->rec_len;
        }
    }

    return -ENOENT;  // Entry not found
}

int Ext2Node::find_directory_entry(const std::string& name, uint32_t& block_idx, uint32_t& entry_offset) {
    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    uint32_t dir_blocks = (inode_.i_size + block_size - 1) / block_size;

    for (uint32_t bidx = 0; bidx < dir_blocks; bidx++) {
        uint32_t block_num;
        int ret = get_data_block(bidx, block_num);
        if (ret < 0) {
            continue;
        }

        // Read directory block
        std::vector<uint8_t> block_buffer(block_size);
        ret = ext2fs_->read_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            continue;
        }

        // Scan entries
        uint32_t offset = 0;
        while (offset < block_size) {
            Ext2DirEntry* entry = reinterpret_cast<Ext2DirEntry*>(block_buffer.data() + offset);

            if (entry->rec_len == 0) {
                break;
            }

            if (entry->inode != 0 && entry->name_len == name.length() &&
                std::memcmp(entry->name, name.c_str(), name.length()) == 0) {
                block_idx = bidx;
                entry_offset = offset;
                return 0;
            }

            offset += entry->rec_len;
        }
    }

    return -ENOENT;
}

int Ext2Node::create_new_inode(uint16_t mode, uint32_t& new_inode_num) {
    bool is_directory = (mode & Ext2Inode::EXT2_S_IFMT) == Ext2Inode::EXT2_S_IFDIR;

    // Allocate inode
    int ret = ext2fs_->allocate_inode(inode_num_, is_directory, new_inode_num);
    if (ret < 0) {
        return ret;
    }

    // Initialize new inode
    Ext2Inode new_inode;
    std::memset(&new_inode, 0, sizeof(Ext2Inode));

    new_inode.i_mode = mode;
    new_inode.i_uid = inode_.i_uid;  // Inherit owner from parent
    new_inode.i_gid = inode_.i_gid;  // Inherit group from parent
    new_inode.i_size = 0;
    new_inode.i_links_count = is_directory ? 2 : 1;  // Directories have "." link

    uint32_t current_time = static_cast<uint32_t>(get_current_time());
    new_inode.i_atime = current_time;
    new_inode.i_ctime = current_time;
    new_inode.i_mtime = current_time;
    new_inode.i_dtime = 0;
    new_inode.i_blocks = 0;

    // Write inode to disk
    ret = ext2fs_->write_inode(new_inode_num, new_inode);
    if (ret < 0) {
        ext2fs_->free_inode(new_inode_num);  // Cleanup
        return ret;
    }

    return 0;
}

// ============================================================================
// File and Directory Operations
// ============================================================================
int Ext2Node::write(const void* buffer, size_t size, uint64_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this is a regular file
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFREG) {
        return -EISDIR;
    }

    if (size == 0) {
        return 0;
    }

    uint32_t block_size = ext2fs_->get_block_size();
    size_t bytes_written = 0;
    const uint8_t* src = static_cast<const uint8_t*>(buffer);

    while (bytes_written < size) {
        // Calculate block index and offset within block
        uint32_t block_idx = (offset + bytes_written) / block_size;
        uint32_t block_offset = (offset + bytes_written) % block_size;
        size_t bytes_to_write = std::min(size - bytes_written, (size_t)(block_size - block_offset));

        // Allocate block if needed
        uint32_t block_num = 0;
        int ret = allocate_data_block(block_idx, block_num);
        if (ret < 0) {
            return ret < 0 ? ret : bytes_written;
        }

        // If partial block write, need to read-modify-write
        std::vector<uint8_t> block_buffer(block_size);
        if (block_offset != 0 || bytes_to_write != block_size) {
            // Read existing data
            ret = ext2fs_->read_data_block(block_num, block_buffer.data());
            if (ret < 0) {
                return ret;
            }
        }

        // Copy new data
        std::memcpy(block_buffer.data() + block_offset, src + bytes_written, bytes_to_write);

        // Write block
        ret = ext2fs_->write_data_block(block_num, block_buffer.data());
        if (ret < 0) {
            return ret;
        }

        bytes_written += bytes_to_write;
    }

    // Update file size if we extended it
    if (offset + size > inode_.i_size) {
        inode_.i_size = offset + size;
    }

    // Update block count (512-byte blocks)
    uint32_t total_blocks = (inode_.i_size + 511) / 512;
    inode_.i_blocks = total_blocks;

    // Update modification and change times
    update_times(false, true, true);

    // Mark inode dirty
    dirty_ = true;

    return bytes_written;
}
int Ext2Node::truncate(uint64_t new_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this is a regular file
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFREG) {
        return -EISDIR;
    }

    if (new_size == inode_.i_size) {
        return 0;  // No change
    }

    uint32_t block_size = ext2fs_->get_block_size();

    if (new_size < inode_.i_size) {
        // Shrinking: free blocks beyond new size
        uint32_t old_blocks = (inode_.i_size + block_size - 1) / block_size;
        uint32_t new_blocks = (new_size + block_size - 1) / block_size;

        if (new_blocks < old_blocks) {
            // Free blocks from new_blocks to old_blocks
            free_data_blocks(new_blocks, old_blocks - new_blocks);
        }

        // Update file size
        inode_.i_size = new_size;
    } else {
        // Extending: just update size (blocks allocated on write)
        inode_.i_size = new_size;
    }

    // Update block count (512-byte blocks)
    uint32_t total_blocks = (inode_.i_size + 511) / 512;
    inode_.i_blocks = total_blocks;

    // Update times
    update_times(false, true, true);

    dirty_ = true;
    return 0;
}

int Ext2Node::sync() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dirty_) {
        int ret = save_inode();
        if (ret < 0) {
            return ret;
        }
        dirty_ = false;
    }

    return 0;
}
int Ext2Node::create(const std::string& name, FilePermissions perms) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if parent is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    // Check if file already exists
    VNode* existing = lookup(name);
    if (existing) {
        existing->unref();
        return -EEXIST;
    }

    // Create new inode for regular file
    uint16_t mode = Ext2Inode::EXT2_S_IFREG | (perms.mode & 0x0FFF);
    uint32_t new_inode_num;
    int ret = create_new_inode(mode, new_inode_num);
    if (ret < 0) {
        return ret;
    }

    // Add directory entry
    ret = add_directory_entry(name, new_inode_num, Ext2DirEntry::EXT2_FT_REG_FILE);
    if (ret < 0) {
        // Cleanup: free the inode
        ext2fs_->free_inode(new_inode_num);
        return ret;
    }

    // Success!
    return 0;
}

int Ext2Node::mkdir(const std::string& name, FilePermissions perms) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if parent is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    // Check if directory already exists
    VNode* existing = lookup(name);
    if (existing) {
        existing->unref();
        return -EEXIST;
    }

    // Create new inode for directory
    uint16_t mode = Ext2Inode::EXT2_S_IFDIR | (perms.mode & 0x0FFF);
    uint32_t new_inode_num;
    int ret = create_new_inode(mode, new_inode_num);
    if (ret < 0) {
        return ret;
    }

    // Load the new inode to modify it
    Ext2Inode new_inode;
    ret = ext2fs_->read_inode(new_inode_num, new_inode);
    if (ret < 0) {
        ext2fs_->free_inode(new_inode_num);
        return ret;
    }

    // Allocate first block for directory
    uint32_t dir_block_num;
    uint32_t group = ext2fs_->get_block_group(new_inode_num);
    ret = ext2fs_->allocate_block(group, dir_block_num);
    if (ret < 0) {
        ext2fs_->free_inode(new_inode_num);
        return ret;
    }

    new_inode.i_block[0] = dir_block_num;
    new_inode.i_size = ext2fs_->get_block_size();
    new_inode.i_blocks = (new_inode.i_size + 511) / 512;

    // Create "." and ".." entries
    uint32_t block_size = ext2fs_->get_block_size();
    std::vector<uint8_t> block_buffer(block_size, 0);

    // Create "." entry
    Ext2DirEntry* dot = reinterpret_cast<Ext2DirEntry*>(block_buffer.data());
    dot->inode = new_inode_num;
    dot->rec_len = calculate_rec_len(1);
    dot->name_len = 1;
    dot->file_type = Ext2DirEntry::EXT2_FT_DIR;
    dot->name[0] = '.';

    // Create ".." entry
    Ext2DirEntry* dotdot = reinterpret_cast<Ext2DirEntry*>(
        block_buffer.data() + dot->rec_len);
    dotdot->inode = inode_num_;  // Parent directory
    dotdot->rec_len = block_size - dot->rec_len;  // Rest of block
    dotdot->name_len = 2;
    dotdot->file_type = Ext2DirEntry::EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    // Write directory block
    ret = ext2fs_->write_data_block(dir_block_num, block_buffer.data());
    if (ret < 0) {
        ext2fs_->free_block(dir_block_num);
        ext2fs_->free_inode(new_inode_num);
        return ret;
    }

    // Write new inode
    ret = ext2fs_->write_inode(new_inode_num, new_inode);
    if (ret < 0) {
        ext2fs_->free_block(dir_block_num);
        ext2fs_->free_inode(new_inode_num);
        return ret;
    }

    // Add directory entry to parent
    ret = add_directory_entry(name, new_inode_num, Ext2DirEntry::EXT2_FT_DIR);
    if (ret < 0) {
        // Cleanup
        ext2fs_->free_block(dir_block_num);
        ext2fs_->free_inode(new_inode_num);
        return ret;
    }

    // Increment parent link count (for ".." in new directory)
    inode_.i_links_count++;
    dirty_ = true;

    return 0;
}

int Ext2Node::remove(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    // Find the file
    VNode* target = lookup(name);
    if (!target) {
        return -ENOENT;
    }

    Ext2Node* ext2_target = dynamic_cast<Ext2Node*>(target);
    if (!ext2_target) {
        target->unref();
        return -EINVAL;
    }

    // Check it's not a directory
    if ((ext2_target->inode_.i_mode & Ext2Inode::EXT2_S_IFMT) == Ext2Inode::EXT2_S_IFDIR) {
        target->unref();
        return -EISDIR;  // Use rmdir for directories
    }

    uint32_t target_inode_num = ext2_target->inode_num_;
    target->unref();

    // Remove directory entry
    int ret = remove_directory_entry(name);
    if (ret < 0) {
        return ret;
    }

    // Load target inode and decrement link count
    Ext2Inode target_inode;
    ret = ext2fs_->read_inode(target_inode_num, target_inode);
    if (ret < 0) {
        return ret;
    }

    target_inode.i_links_count--;

    // If link count reaches 0, free the inode and all its blocks
    if (target_inode.i_links_count == 0) {
        // Mark as deleted
        target_inode.i_dtime = static_cast<uint32_t>(get_current_time());

        // Free all data blocks
        uint32_t block_size = ext2fs_->get_block_size();
        uint32_t num_blocks = (target_inode.i_size + block_size - 1) / block_size;

        // Free direct blocks
        for (uint32_t i = 0; i < Ext2Inode::EXT2_NDIR_BLOCKS && i < num_blocks; i++) {
            if (target_inode.i_block[i] != 0) {
                ext2fs_->free_block(target_inode.i_block[i]);
                target_inode.i_block[i] = 0;
            }
        }

        // Free single indirect block and its data blocks
        if (target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK] != 0) {
            uint32_t blocks_per_indirect = block_size / sizeof(uint32_t);
            std::vector<uint32_t> indirect(blocks_per_indirect);

            if (ext2fs_->read_data_block(target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                        indirect.data()) == 0) {
                // Free each data block pointed to by indirect block
                for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                    if (indirect[i] != 0) {
                        ext2fs_->free_block(indirect[i]);
                    }
                }
            }

            // Free the indirect block itself
            ext2fs_->free_block(target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK]);
            target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK] = 0;
        }

        // Free double indirect block and its indirect/data blocks
        if (target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK] != 0) {
            uint32_t blocks_per_indirect = block_size / sizeof(uint32_t);
            std::vector<uint32_t> dindirect(blocks_per_indirect);

            if (ext2fs_->read_data_block(target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                        dindirect.data()) == 0) {
                // Free each indirect block and its data blocks
                for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                    if (dindirect[i] != 0) {
                        std::vector<uint32_t> indirect(blocks_per_indirect);
                        if (ext2fs_->read_data_block(dindirect[i], indirect.data()) == 0) {
                            // Free data blocks
                            for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                                if (indirect[j] != 0) {
                                    ext2fs_->free_block(indirect[j]);
                                }
                            }
                        }
                        // Free indirect block
                        ext2fs_->free_block(dindirect[i]);
                    }
                }
            }

            // Free the double indirect block itself
            ext2fs_->free_block(target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK]);
            target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK] = 0;
        }

        // Free triple indirect block and its double indirect/indirect/data blocks
        if (target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK] != 0) {
            uint32_t blocks_per_indirect = block_size / sizeof(uint32_t);
            std::vector<uint32_t> tindirect(blocks_per_indirect);

            if (ext2fs_->read_data_block(target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                        tindirect.data()) == 0) {
                // Free each double indirect block and its contents
                for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                    if (tindirect[i] != 0) {
                        std::vector<uint32_t> dindirect(blocks_per_indirect);
                        if (ext2fs_->read_data_block(tindirect[i], dindirect.data()) == 0) {
                            // Free each indirect block and its data blocks
                            for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                                if (dindirect[j] != 0) {
                                    std::vector<uint32_t> indirect(blocks_per_indirect);
                                    if (ext2fs_->read_data_block(dindirect[j], indirect.data()) == 0) {
                                        // Free data blocks
                                        for (uint32_t k = 0; k < blocks_per_indirect; k++) {
                                            if (indirect[k] != 0) {
                                                ext2fs_->free_block(indirect[k]);
                                            }
                                        }
                                    }
                                    // Free indirect block
                                    ext2fs_->free_block(dindirect[j]);
                                }
                            }
                        }
                        // Free double indirect block
                        ext2fs_->free_block(tindirect[i]);
                    }
                }
            }

            // Free the triple indirect block itself
            ext2fs_->free_block(target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK]);
            target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK] = 0;
        }

        // Write updated inode and free it
        ext2fs_->write_inode(target_inode_num, target_inode);
        ext2fs_->free_inode(target_inode_num);
    } else {
        // Still has links, just update link count
        ext2fs_->write_inode(target_inode_num, target_inode);
    }

    return 0;
}

int Ext2Node::rmdir(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this is a directory
    if ((inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        return -ENOTDIR;
    }

    // Find the directory
    VNode* target = lookup(name);
    if (!target) {
        return -ENOENT;
    }

    Ext2Node* ext2_target = dynamic_cast<Ext2Node*>(target);
    if (!ext2_target) {
        target->unref();
        return -EINVAL;
    }

    // Check it's a directory
    if ((ext2_target->inode_.i_mode & Ext2Inode::EXT2_S_IFMT) != Ext2Inode::EXT2_S_IFDIR) {
        target->unref();
        return -ENOTDIR;
    }

    // Check if directory is empty (only "." and "..")
    auto entries = ext2_target->readdir();
    bool is_empty = true;
    for (const auto& entry : entries) {
        if (entry != "." && entry != "..") {
            is_empty = false;
            break;
        }
    }

    if (!is_empty) {
        target->unref();
        return -ENOTEMPTY;
    }

    uint32_t target_inode_num = ext2_target->inode_num_;
    target->unref();

    // Remove directory entry from parent
    int ret = remove_directory_entry(name);
    if (ret < 0) {
        return ret;
    }

    // Decrement parent link count (remove ".." link)
    inode_.i_links_count--;
    dirty_ = true;

    // Free the directory inode and its blocks
    Ext2Inode target_inode;
    ret = ext2fs_->read_inode(target_inode_num, target_inode);
    if (ret == 0) {
        // Mark as deleted
        target_inode.i_dtime = static_cast<uint32_t>(get_current_time());

        // Free all directory data blocks
        uint32_t block_size = ext2fs_->get_block_size();
        uint32_t num_blocks = (target_inode.i_size + block_size - 1) / block_size;

        // Free direct blocks
        for (uint32_t i = 0; i < Ext2Inode::EXT2_NDIR_BLOCKS && i < num_blocks; i++) {
            if (target_inode.i_block[i] != 0) {
                ext2fs_->free_block(target_inode.i_block[i]);
                target_inode.i_block[i] = 0;
            }
        }

        // Free single indirect block and its data blocks
        if (target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK] != 0) {
            uint32_t blocks_per_indirect = block_size / sizeof(uint32_t);
            std::vector<uint32_t> indirect(blocks_per_indirect);

            if (ext2fs_->read_data_block(target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK],
                                        indirect.data()) == 0) {
                // Free each data block pointed to by indirect block
                for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                    if (indirect[i] != 0) {
                        ext2fs_->free_block(indirect[i]);
                    }
                }
            }

            // Free the indirect block itself
            ext2fs_->free_block(target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK]);
            target_inode.i_block[Ext2Inode::EXT2_IND_BLOCK] = 0;
        }

        // Free double indirect block and its indirect/data blocks
        if (target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK] != 0) {
            uint32_t blocks_per_indirect = block_size / sizeof(uint32_t);
            std::vector<uint32_t> dindirect(blocks_per_indirect);

            if (ext2fs_->read_data_block(target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK],
                                        dindirect.data()) == 0) {
                // Free each indirect block and its data blocks
                for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                    if (dindirect[i] != 0) {
                        std::vector<uint32_t> indirect(blocks_per_indirect);
                        if (ext2fs_->read_data_block(dindirect[i], indirect.data()) == 0) {
                            // Free data blocks
                            for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                                if (indirect[j] != 0) {
                                    ext2fs_->free_block(indirect[j]);
                                }
                            }
                        }
                        // Free indirect block
                        ext2fs_->free_block(dindirect[i]);
                    }
                }
            }

            // Free the double indirect block itself
            ext2fs_->free_block(target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK]);
            target_inode.i_block[Ext2Inode::EXT2_DIND_BLOCK] = 0;
        }

        // Free triple indirect block and its double indirect/indirect/data blocks
        if (target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK] != 0) {
            uint32_t blocks_per_indirect = block_size / sizeof(uint32_t);
            std::vector<uint32_t> tindirect(blocks_per_indirect);

            if (ext2fs_->read_data_block(target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK],
                                        tindirect.data()) == 0) {
                // Free each double indirect block and its contents
                for (uint32_t i = 0; i < blocks_per_indirect; i++) {
                    if (tindirect[i] != 0) {
                        std::vector<uint32_t> dindirect(blocks_per_indirect);
                        if (ext2fs_->read_data_block(tindirect[i], dindirect.data()) == 0) {
                            // Free each indirect block and its data blocks
                            for (uint32_t j = 0; j < blocks_per_indirect; j++) {
                                if (dindirect[j] != 0) {
                                    std::vector<uint32_t> indirect(blocks_per_indirect);
                                    if (ext2fs_->read_data_block(dindirect[j], indirect.data()) == 0) {
                                        // Free data blocks
                                        for (uint32_t k = 0; k < blocks_per_indirect; k++) {
                                            if (indirect[k] != 0) {
                                                ext2fs_->free_block(indirect[k]);
                                            }
                                        }
                                    }
                                    // Free indirect block
                                    ext2fs_->free_block(dindirect[j]);
                                }
                            }
                        }
                        // Free double indirect block
                        ext2fs_->free_block(tindirect[i]);
                    }
                }
            }

            // Free the triple indirect block itself
            ext2fs_->free_block(target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK]);
            target_inode.i_block[Ext2Inode::EXT2_TIND_BLOCK] = 0;
        }

        // Write updated inode and free it
        ext2fs_->write_inode(target_inode_num, target_inode);
    }

    ext2fs_->free_inode(target_inode_num);

    return 0;
}
int Ext2Node::link(const std::string& name, VNode* target) { return -EROFS; }
int Ext2Node::symlink(const std::string& name, const std::string& target) { return -EROFS; }
int Ext2Node::rename(const std::string& oldname, const std::string& newname) { return -EROFS; }

uint64_t Ext2Node::get_current_time() {
    return static_cast<uint64_t>(time(nullptr));
}

void Ext2Node::update_times(bool update_atime, bool update_mtime, bool update_ctime) {
    uint32_t current_time = static_cast<uint32_t>(get_current_time());

    if (update_atime) {
        inode_.i_atime = current_time;
    }
    if (update_mtime) {
        inode_.i_mtime = current_time;
    }
    if (update_ctime) {
        inode_.i_ctime = current_time;
    }

    if (update_atime || update_mtime || update_ctime) {
        dirty_ = true;
    }
}

} // namespace xinim::vfs

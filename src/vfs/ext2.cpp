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

// Error codes
#define EINVAL 22  // Invalid argument
#define EIO    5   // I/O error
#define ENOENT 2   // No such file or directory
#define EISDIR 21  // Is a directory
#define ENOTDIR 20 // Not a directory
#define EROFS  30  // Read-only filesystem
#define ENOSPC 28  // No space left on device

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
    // TODO: Implement when adding write support
    // For now, read-only filesystem
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
    // TODO: Implement when adding write support
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
    // TODO: Implement when adding write support
    return -EROFS;
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
    // TODO: Implement when adding write support
    return -EROFS;
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

// Unimplemented write operations (read-only for now)
int Ext2Node::write(const void* buffer, size_t size, uint64_t offset) { return -EROFS; }
int Ext2Node::truncate(uint64_t size) { return -EROFS; }
int Ext2Node::sync() { return 0; }
int Ext2Node::create(const std::string& name, FilePermissions perms) { return -EROFS; }
int Ext2Node::mkdir(const std::string& name, FilePermissions perms) { return -EROFS; }
int Ext2Node::remove(const std::string& name) { return -EROFS; }
int Ext2Node::rmdir(const std::string& name) { return -EROFS; }
int Ext2Node::link(const std::string& name, VNode* target) { return -EROFS; }
int Ext2Node::symlink(const std::string& name, const std::string& target) { return -EROFS; }
int Ext2Node::rename(const std::string& oldname, const std::string& newname) { return -EROFS; }

void Ext2Node::update_times(bool atime, bool mtime, bool ctime) {
    // TODO: Implement when adding write support
}

} // namespace xinim::vfs

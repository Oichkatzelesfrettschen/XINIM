/**
 * @file ext2.hpp
 * @brief ext2 - Second Extended Filesystem Driver
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Implements ext2 filesystem support for persistent storage on block devices.
 * Based on the official ext2 specification and Linux kernel implementation.
 *
 * References:
 * - https://www.nongnu.org/ext2-doc/ext2.html
 * - https://docs.kernel.org/filesystems/ext2.html
 * - http://wiki.osdev.org/Ext2
 */

#pragma once

#include <xinim/vfs/vfs.hpp>
#include <xinim/vfs/filesystem.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace xinim::vfs {

// Forward declarations
class Ext2Filesystem;
class Ext2Node;

// ============================================================================
// ext2 On-Disk Structures
// ============================================================================

/**
 * @brief ext2 Superblock
 *
 * Located at byte offset 1024 from the start of the device.
 * Contains filesystem metadata and configuration.
 */
struct __attribute__((packed)) Ext2Superblock {
    // Base fields (Revision 0)
    uint32_t s_inodes_count;        // Total inodes
    uint32_t s_blocks_count;        // Total blocks
    uint32_t s_r_blocks_count;      // Reserved blocks (for superuser)
    uint32_t s_free_blocks_count;   // Free blocks
    uint32_t s_free_inodes_count;   // Free inodes
    uint32_t s_first_data_block;    // First data block (0 for 1KB blocks, 1 for larger)
    uint32_t s_log_block_size;      // Block size = 1024 << s_log_block_size
    uint32_t s_log_frag_size;       // Fragment size (unused, same as block size)
    uint32_t s_blocks_per_group;    // Blocks per block group
    uint32_t s_frags_per_group;     // Fragments per group (unused)
    uint32_t s_inodes_per_group;    // Inodes per block group
    uint32_t s_mtime;               // Last mount time (Unix timestamp)
    uint32_t s_wtime;               // Last write time (Unix timestamp)
    uint16_t s_mnt_count;           // Mount count since last fsck
    uint16_t s_max_mnt_count;       // Max mounts before fsck required
    uint16_t s_magic;               // Magic signature (0xEF53)
    uint16_t s_state;               // Filesystem state (1 = clean, 2 = errors)
    uint16_t s_errors;              // Error handling behavior
    uint16_t s_minor_rev_level;     // Minor revision level
    uint32_t s_lastcheck;           // Last fsck time
    uint32_t s_checkinterval;       // Max time between fscks
    uint32_t s_creator_os;          // Creator OS (0=Linux, 1=Hurd, etc.)
    uint32_t s_rev_level;           // Revision level (0=original, 1=dynamic)
    uint16_t s_def_resuid;          // Default UID for reserved blocks
    uint16_t s_def_resgid;          // Default GID for reserved blocks

    // Extended fields (Revision 1)
    uint32_t s_first_ino;           // First non-reserved inode (usually 11)
    uint16_t s_inode_size;          // Size of inode structure (usually 128)
    uint16_t s_block_group_nr;      // Block group number of this superblock
    uint32_t s_feature_compat;      // Compatible features
    uint32_t s_feature_incompat;    // Incompatible features
    uint32_t s_feature_ro_compat;   // Read-only compatible features
    uint8_t  s_uuid[16];            // Volume UUID
    char     s_volume_name[16];     // Volume name
    char     s_last_mounted[64];    // Last mount point
    uint32_t s_algo_bitmap;         // Compression algorithm

    // Performance hints
    uint8_t  s_prealloc_blocks;     // Blocks to preallocate for files
    uint8_t  s_prealloc_dir_blocks; // Blocks to preallocate for directories
    uint16_t s_padding1;

    // Journaling support (ext3)
    uint8_t  s_journal_uuid[16];    // Journal UUID
    uint32_t s_journal_inum;        // Journal inode number
    uint32_t s_journal_dev;         // Journal device number
    uint32_t s_last_orphan;         // Head of orphan inode list

    uint32_t s_reserved[197];       // Padding to 1024 bytes

    // Magic number
    static constexpr uint16_t EXT2_MAGIC = 0xEF53;

    // Filesystem states
    static constexpr uint16_t EXT2_VALID_FS = 1;  // Cleanly unmounted
    static constexpr uint16_t EXT2_ERROR_FS = 2;  // Errors detected

    // Revision levels
    static constexpr uint32_t EXT2_GOOD_OLD_REV = 0;  // Original format
    static constexpr uint32_t EXT2_DYNAMIC_REV = 1;   // V2 with dynamic inode sizes

    // Default inode size
    static constexpr uint16_t EXT2_GOOD_OLD_INODE_SIZE = 128;

    // First non-reserved inode
    static constexpr uint32_t EXT2_GOOD_OLD_FIRST_INO = 11;
};

/**
 * @brief ext2 Block Group Descriptor
 *
 * Each block group has a descriptor that tracks its allocation bitmaps
 * and inode table location.
 */
struct __attribute__((packed)) Ext2GroupDescriptor {
    uint32_t bg_block_bitmap;       // Block number of block bitmap
    uint32_t bg_inode_bitmap;       // Block number of inode bitmap
    uint32_t bg_inode_table;        // Block number of inode table
    uint16_t bg_free_blocks_count;  // Free blocks in group
    uint16_t bg_free_inodes_count;  // Free inodes in group
    uint16_t bg_used_dirs_count;    // Number of directories in group
    uint16_t bg_pad;                // Padding
    uint32_t bg_reserved[3];        // Reserved for future use
};

/**
 * @brief ext2 Inode
 *
 * Represents a file, directory, or special file on disk.
 */
struct __attribute__((packed)) Ext2Inode {
    uint16_t i_mode;                // File mode (type and permissions)
    uint16_t i_uid;                 // Owner UID (low 16 bits)
    uint32_t i_size;                // File size in bytes (low 32 bits)
    uint32_t i_atime;               // Access time
    uint32_t i_ctime;               // Creation time
    uint32_t i_mtime;               // Modification time
    uint32_t i_dtime;               // Deletion time
    uint16_t i_gid;                 // Group GID (low 16 bits)
    uint16_t i_links_count;         // Hard link count
    uint32_t i_blocks;              // Block count (512-byte blocks)
    uint32_t i_flags;               // File flags
    uint32_t i_osd1;                // OS-dependent value 1
    uint32_t i_block[15];           // Block pointers (0-11 direct, 12 indirect, 13 double, 14 triple)
    uint32_t i_generation;          // File version (for NFS)
    uint32_t i_file_acl;            // File ACL
    uint32_t i_dir_acl;             // Directory ACL / size high 32 bits
    uint32_t i_faddr;               // Fragment address
    uint8_t  i_osd2[12];            // OS-dependent value 2

    // File mode bits
    static constexpr uint16_t EXT2_S_IFSOCK = 0xC000;  // Socket
    static constexpr uint16_t EXT2_S_IFLNK  = 0xA000;  // Symbolic link
    static constexpr uint16_t EXT2_S_IFREG  = 0x8000;  // Regular file
    static constexpr uint16_t EXT2_S_IFBLK  = 0x6000;  // Block device
    static constexpr uint16_t EXT2_S_IFDIR  = 0x4000;  // Directory
    static constexpr uint16_t EXT2_S_IFCHR  = 0x2000;  // Character device
    static constexpr uint16_t EXT2_S_IFIFO  = 0x1000;  // FIFO
    static constexpr uint16_t EXT2_S_IFMT   = 0xF000;  // Format mask

    // Permission bits
    static constexpr uint16_t EXT2_S_ISUID  = 0x0800;  // SUID
    static constexpr uint16_t EXT2_S_ISGID  = 0x0400;  // SGID
    static constexpr uint16_t EXT2_S_ISVTX  = 0x0200;  // Sticky bit
    static constexpr uint16_t EXT2_S_IRWXU  = 0x01C0;  // User RWX
    static constexpr uint16_t EXT2_S_IRUSR  = 0x0100;  // User read
    static constexpr uint16_t EXT2_S_IWUSR  = 0x0080;  // User write
    static constexpr uint16_t EXT2_S_IXUSR  = 0x0040;  // User execute
    static constexpr uint16_t EXT2_S_IRWXG  = 0x0038;  // Group RWX
    static constexpr uint16_t EXT2_S_IRGRP  = 0x0020;  // Group read
    static constexpr uint16_t EXT2_S_IWGRP  = 0x0010;  // Group write
    static constexpr uint16_t EXT2_S_IXGRP  = 0x0008;  // Group execute
    static constexpr uint16_t EXT2_S_IRWXO  = 0x0007;  // Other RWX
    static constexpr uint16_t EXT2_S_IROTH  = 0x0004;  // Other read
    static constexpr uint16_t EXT2_S_IWOTH  = 0x0002;  // Other write
    static constexpr uint16_t EXT2_S_IXOTH  = 0x0001;  // Other execute

    // Block pointer indices
    static constexpr int EXT2_NDIR_BLOCKS = 12;   // Direct blocks
    static constexpr int EXT2_IND_BLOCK   = 12;   // Indirect block
    static constexpr int EXT2_DIND_BLOCK  = 13;   // Double indirect
    static constexpr int EXT2_TIND_BLOCK  = 14;   // Triple indirect
};

/**
 * @brief ext2 Directory Entry
 *
 * Variable-length structure stored in directory data blocks.
 */
struct __attribute__((packed)) Ext2DirEntry {
    uint32_t inode;         // Inode number (0 = unused entry)
    uint16_t rec_len;       // Entry length (to next entry)
    uint8_t  name_len;      // Name length
    uint8_t  file_type;     // File type (if feature enabled)
    char     name[1];       // File name (variable length, up to 255)

    // File types
    static constexpr uint8_t EXT2_FT_UNKNOWN  = 0;
    static constexpr uint8_t EXT2_FT_REG_FILE = 1;
    static constexpr uint8_t EXT2_FT_DIR      = 2;
    static constexpr uint8_t EXT2_FT_CHRDEV   = 3;
    static constexpr uint8_t EXT2_FT_BLKDEV   = 4;
    static constexpr uint8_t EXT2_FT_FIFO     = 5;
    static constexpr uint8_t EXT2_FT_SOCK     = 6;
    static constexpr uint8_t EXT2_FT_SYMLINK  = 7;

    // Maximum name length
    static constexpr size_t EXT2_NAME_LEN = 255;
};

// ============================================================================
// ext2 VNode
// ============================================================================

/**
 * @brief ext2 VNode representing a file or directory
 */
class Ext2Node : public VNode {
public:
    Ext2Node(Ext2Filesystem* fs, uint32_t inode_num);
    virtual ~Ext2Node();

    // VNode interface
    int read(void* buffer, size_t size, uint64_t offset) override;
    int write(const void* buffer, size_t size, uint64_t offset) override;
    int truncate(uint64_t size) override;
    int sync() override;

    std::vector<std::string> readdir() override;
    VNode* lookup(const std::string& name) override;
    int create(const std::string& name, FilePermissions perms) override;
    int mkdir(const std::string& name, FilePermissions perms) override;
    int remove(const std::string& name) override;
    int rmdir(const std::string& name) override;
    int link(const std::string& name, VNode* target) override;
    int symlink(const std::string& name, const std::string& target) override;
    int rename(const std::string& oldname, const std::string& newname) override;

    FileAttributes get_attributes() override;
    int set_attributes(const FileAttributes& attrs) override;

    void ref() override;
    void unref() override;

    // ext2-specific
    uint32_t get_inode_num() const { return inode_num_; }
    const Ext2Inode& get_inode() const { return inode_; }

private:
    Ext2Filesystem* ext2fs_;
    uint32_t inode_num_;
    Ext2Inode inode_;
    int ref_count_;
    bool dirty_;

    mutable std::mutex mutex_;

    // Helper methods
    int load_inode();
    int save_inode();
    int read_block_data(uint32_t block_idx, void* buffer);
    int get_data_block(uint32_t block_idx, uint32_t& block_num);
    FileType inode_type_to_file_type() const;
    void update_times(bool atime, bool mtime, bool ctime);
};

// ============================================================================
// ext2 Filesystem
// ============================================================================

/**
 * @brief ext2 Filesystem Driver
 *
 * Implements the Second Extended Filesystem for persistent block storage.
 */
class Ext2Filesystem : public BlockFilesystem {
public:
    Ext2Filesystem();
    virtual ~Ext2Filesystem();

    // FileSystem interface
    std::string get_type() const override { return "ext2"; }

    // ext2-specific
    uint32_t get_block_size() const { return block_size_; }
    uint32_t get_blocks_per_group() const { return superblock_.s_blocks_per_group; }
    uint32_t get_inodes_per_group() const { return superblock_.s_inodes_per_group; }
    uint32_t get_inode_size() const { return inode_size_; }

    // Internal methods (called by Ext2Node)
    int read_inode(uint32_t inode_num, Ext2Inode& inode);
    int write_inode(uint32_t inode_num, const Ext2Inode& inode);
    int read_data_block(uint32_t block_num, void* buffer);
    int write_data_block(uint32_t block_num, const void* buffer);

    const Ext2Superblock& get_superblock() const { return superblock_; }
    const std::vector<Ext2GroupDescriptor>& get_group_descriptors() const { return group_descriptors_; }

protected:
    // BlockFilesystem interface
    int read_superblock() override;
    int write_superblock() override;
    VNode* create_root_vnode() override;
    int flush_all() override;

private:
    Ext2Superblock superblock_;
    std::vector<Ext2GroupDescriptor> group_descriptors_;

    uint32_t block_size_;
    uint32_t inode_size_;
    uint32_t num_block_groups_;
    uint32_t superblock_block_;  // Block number where superblock is located

    // Helper methods
    int validate_superblock();
    int read_group_descriptors();
    uint32_t get_block_group(uint32_t inode_num) const;
    uint32_t get_group_index(uint32_t inode_num) const;
    uint64_t get_inode_offset(uint32_t inode_num) const;

    friend class Ext2Node;
};

} // namespace xinim::vfs

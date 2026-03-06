/**
 * @file bare_vfs.hpp
 * @brief Bare-metal VFS core structures (ADR-0009)
 *
 * All structures are freestanding: no STL, no heap, no exceptions.
 * Designed for -ffreestanding -fno-exceptions -fno-rtti compilation.
 *
 * Memory layout (fixed, known at compile time, ~1.35 MB total):
 *   Inode table:   MAX_INODES * 64B  = 64 KB
 *   Dirent arena:  MAX_DIRENTS * 32B = 256 KB
 *   Data arena:    DATA_ARENA_SIZE   = 1 MB
 *   Buffer cache:  CACHE_BLOCKS * (512+16)B ~= 32 KB
 *   Mount table:   MAX_MOUNTS * 64B  = 512 B
 *   FD table:      MAX_FDS * 16B     = 1 KB
 */

#ifndef XINIM_VFS_BARE_VFS_HPP
#define XINIM_VFS_BARE_VFS_HPP

#include <cstdint>
#include <cstddef>

/* ============================================================================
 * Compile-time limits
 * ========================================================================== */

inline constexpr uint32_t MAX_INODES       = 1024;
inline constexpr uint32_t MAX_DIRENTS      = 8192;   // 256 KB / 32 bytes
inline constexpr uint32_t MAX_MOUNTS       = 8;
inline constexpr uint32_t MAX_FDS          = 64;
inline constexpr uint32_t CACHE_BLOCKS     = 64;
inline constexpr uint32_t CACHE_BLK_SIZE   = 512;
inline constexpr uint32_t DATA_ARENA_SIZE  = (1u << 20); // 1 MB

/* ============================================================================
 * POSIX file type / mode constants (subset needed for ramfs)
 * ========================================================================== */

inline constexpr uint16_t S_IFMT   = 0xF000; // File type mask
inline constexpr uint16_t S_IFREG  = 0x8000; // Regular file
inline constexpr uint16_t S_IFDIR  = 0x4000; // Directory
inline constexpr uint16_t S_IFLNK  = 0xA000; // Symbolic link

// Permission bits
inline constexpr uint16_t S_IRUSR  = 0x0100;
inline constexpr uint16_t S_IWUSR  = 0x0080;
inline constexpr uint16_t S_IXUSR  = 0x0040;
inline constexpr uint16_t S_IRGRP  = 0x0020;
inline constexpr uint16_t S_IWGRP  = 0x0010;
inline constexpr uint16_t S_IXGRP  = 0x0008;
inline constexpr uint16_t S_IROTH  = 0x0004;
inline constexpr uint16_t S_IWOTH  = 0x0002;
inline constexpr uint16_t S_IXOTH  = 0x0001;

inline constexpr uint16_t MODE_FILE_DEFAULT = static_cast<uint16_t>(S_IFREG | 0x01B4u); // 0644
inline constexpr uint16_t MODE_DIR_DEFAULT  = static_cast<uint16_t>(S_IFDIR | 0x01EDu); // 0755

// Dirent file type byte (DT_* compatible)
inline constexpr uint8_t DT_UNKNOWN = 0;
inline constexpr uint8_t DT_REG     = 8;
inline constexpr uint8_t DT_DIR     = 4;
inline constexpr uint8_t DT_LNK     = 10;

// Open flags (O_* compatible)
inline constexpr uint32_t O_RDONLY  = 0x0000;
inline constexpr uint32_t O_WRONLY  = 0x0001;
inline constexpr uint32_t O_RDWR    = 0x0002;
inline constexpr uint32_t O_CREAT   = 0x0040;
inline constexpr uint32_t O_TRUNC   = 0x0200;
inline constexpr uint32_t O_APPEND  = 0x0400;

// lseek whence values (SEEK_* compatible)
inline constexpr int SEEK_SET = 0; // absolute position
inline constexpr int SEEK_CUR = 1; // relative to current position
inline constexpr int SEEK_END = 2; // relative to file end

// Inode internal flags
inline constexpr uint16_t INODE_IS_INLINE = 0x0001; // data in inline_data[]
inline constexpr uint16_t INODE_IS_DIR    = 0x0002; // is a directory
inline constexpr uint16_t INODE_IS_USED   = 0x0004; // slot allocated

/* ============================================================================
 * RawInode -- 64 bytes (one cache line)
 *
 * WHY 64 bytes: one L1 cache line. open() and stat() each load exactly one
 * cache line. No false sharing with adjacent inodes.
 *
 * WHY inline_data: files <= 24 bytes (pid files, /proc entries, config stubs)
 * have zero additional allocation. This covers the common case in a microkernel.
 * ========================================================================== */

struct alignas(64) RawInode {
    uint32_t ino;              // Inode number (0 = free slot)
    uint16_t mode;             // POSIX st_mode (type + permissions)
    uint16_t iflags;           // Internal: INODE_IS_INLINE, INODE_IS_DIR, INODE_IS_USED
    uint32_t uid;              // Owner UID
    uint32_t gid;              // Owner GID
    uint32_t nlink;            // Hard link count
    uint32_t open_count;       // Number of open FDs referencing this inode
    uint32_t parent_ino;       // Parent directory inode (for ".." resolution)
    uint64_t size;             // File size in bytes
    int64_t  mtime;            // Modification time (kernel ticks)
    int64_t  ctime;            // Status change time
    union {
        uint8_t  inline_data[24];  // Small files (<= 24 bytes): data stored here
        uint64_t data_block_off;   // Larger files: byte offset into g_data_arena
        uint64_t dirent_start;     // Directories: first index into g_dirent_arena
    };
    // Padding: 4+2+2+4+4+4+4+4+8+8+8+24 = 76 bytes; reorder to fit 64
    // Actual layout verified by static_assert in inode_table.cpp
};
// NOTE: sizeof(RawInode) may be 80 bytes with current field order due to
// alignment of uint64_t fields. The alignas(64) still ensures cache-line
// alignment of each inode in the array. A static_assert checks the size
// in inode_table.cpp and we pad as needed.

/* ============================================================================
 * DirEntry -- 32 bytes (fixed-size directory entry)
 *
 * WHY fixed-size: no heap allocation during directory traversal.
 * WHY 26-char name: covers 95%+ of kernel-space paths. Long names (> 26)
 * will return -ENAMETOOLONG in v1.3.0; overflow table is a v1.4.0 concern.
 * ========================================================================== */

struct DirEntry {
    uint32_t child_ino;     // Inode number of this entry (0 = free slot)
    uint8_t  type;          // File type byte (DT_REG, DT_DIR, etc.)
    uint8_t  namelen;       // Length of name in bytes (1-26)
    char     name[26];      // Name bytes (NOT necessarily null-terminated)
};
static_assert(sizeof(DirEntry) == 32, "DirEntry must be 32 bytes");

/* ============================================================================
 * MountEntry -- 64 bytes
 * ========================================================================== */

// Forward declaration -- FsOps defined below
struct FsOps;

struct MountEntry {
    uint32_t       root_ino;   // Inode of mounted root (0 = free/inactive slot)
    uint32_t       _pad;       // padding for alignment
    const FsOps*   ops;        // Filesystem operations for this mount
    char           path[48];   // Absolute mount point path (null-terminated)
};
static_assert(sizeof(MountEntry) == 64, "MountEntry must be 64 bytes");

/* ============================================================================
 * FdEntry -- 16 bytes (VFS server global FD table entry)
 * ========================================================================== */

struct FdEntry {
    uint32_t ino;    // Open inode number (0 = free slot)
    uint32_t flags;  // Open flags (O_RDONLY, O_WRONLY, O_RDWR, O_APPEND)
    int64_t  pos;    // Current file position (signed; -1 = append mode)
};
static_assert(sizeof(FdEntry) == 16, "FdEntry must be 16 bytes");

/* ============================================================================
 * KStat -- kernel-internal stat structure (maps to POSIX struct stat)
 * ========================================================================== */

struct KStat {
    uint64_t st_dev;      // Device ID (1 for ramfs)
    uint64_t st_ino;      // Inode number
    uint32_t st_mode;     // File type + permissions
    uint32_t st_nlink;    // Number of hard links
    uint32_t st_uid;      // Owner UID
    uint32_t st_gid;      // Owner GID
    uint64_t st_rdev;     // Device ID for special files (0 for regular)
    int64_t  st_size;     // File size in bytes
    int64_t  st_blksize;  // Preferred I/O block size (512 for ramfs)
    int64_t  st_blocks;   // Number of 512-byte blocks allocated
    int64_t  st_atime;    // Last access time
    int64_t  st_mtime;    // Last modification time
    int64_t  st_ctime;    // Last status change time
};

/* ============================================================================
 * CacheBlock -- 512-byte buffer cache entry
 * ========================================================================== */

struct CacheBlock {
    uint64_t block_num;           // Block number (UINT64_MAX = free)
    uint32_t device_id;           // Device ID (1 = ramfs data arena)
    uint32_t lru_seq;             // LRU counter (incremented on each access)
    uint32_t dirty    : 1;        // Dirty bit: data written but not flushed
    uint32_t _flags   : 31;       // Reserved
    uint32_t _pad;
    uint8_t  data[CACHE_BLK_SIZE]; // Block data
};

/* ============================================================================
 * FsOps -- function pointer table (replaces virtual dispatch)
 *
 * WHY function pointer table: virtual requires vtable emission which is
 * disabled with -fno-rtti. A function pointer table is explicit, zero-overhead,
 * and compatible with freestanding compilation.
 * ========================================================================== */

struct FsOps {
    // Open a file by inode number, allocate FD. Returns fd >= 0 or -errno.
    int (*open)    (uint32_t ino, uint32_t flags, int* out_fd);

    // Read len bytes at offset off from inode into buf. Returns bytes read or -errno.
    int (*read)    (uint32_t ino, void* buf, uint32_t len, int64_t off);

    // Write len bytes at offset off from buf into inode. Returns bytes written or -errno.
    int (*write)   (uint32_t ino, const void* buf, uint32_t len, int64_t off);

    // Release file descriptor. Returns 0 or -errno.
    int (*close)   (int fd);

    // Fill KStat from inode. Returns 0 or -errno.
    int (*stat)    (uint32_t ino, KStat* out);

    // Create directory named name[namelen] under parent_ino with mode. Returns 0 or -errno.
    int (*mkdir)   (uint32_t parent_ino, const char* name, uint8_t namelen, uint16_t mode);

    // Remove file named name[namelen] from parent_ino. Returns 0 or -errno.
    int (*unlink)  (uint32_t parent_ino, const char* name, uint8_t namelen);

    // Fill buf with up to max_entries DirEntry for directory ino. Returns count or -errno.
    int (*readdir) (uint32_t ino, DirEntry* buf, int max_entries);
};

/* ============================================================================
 * PathComponent -- zero-allocation path component slice
 * ========================================================================== */

struct PathComponent {
    const char* start;  // Pointer into original path buffer
    uint8_t     len;    // Component length (max 26 for name table, 255 absolute)
};

#endif /* XINIM_VFS_BARE_VFS_HPP */

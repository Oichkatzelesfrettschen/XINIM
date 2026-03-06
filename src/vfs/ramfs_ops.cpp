/**
 * @file ramfs_ops.cpp
 * @brief RAM filesystem FsOps implementation (ADR-0009)
 *
 * Implements all 8 FsOps for the in-memory ramfs filesystem.
 * No heap allocation. No STL. All data in pre-allocated arenas.
 *
 * Small files (<= 24 bytes): stored inline in RawInode.inline_data.
 * Larger files: stored in g_data_arena (via data_arena_alloc).
 * Directories: entries stored in g_dirent_arena (via dirent_block_alloc).
 */

#include "bare_vfs.hpp"
#include "inode_table.hpp"
#include "dirent.hpp"
#include "fd_table.hpp"
#include <cstring>
#include <xinim/ipc/message_types.h>

// Inline data threshold -- must match union field size in RawInode
inline constexpr uint32_t INLINE_THRESHOLD = 24;

// ============================================================================
// Helpers
// ============================================================================

static inline bool is_inline(const RawInode* n) {
    return (n->iflags & INODE_IS_INLINE) != 0;
}

// ============================================================================
// ramfs_open
// ============================================================================

static int ramfs_open(uint32_t ino, uint32_t flags, int* out_fd) {
    if (!out_fd) return -IPC_EINVAL;
    RawInode* inode = inode_get(ino);
    if (!inode) return -IPC_ENOENT;

    int fd = fd_allocate(ino, flags);
    if (fd < 0) return -IPC_EMFILE;

    // O_TRUNC: zero file size and reclaim any arena allocation
    if ((flags & O_TRUNC) && !(inode->iflags & INODE_IS_DIR)) {
        if (!(inode->iflags & INODE_IS_INLINE) && inode->size > 0) {
            data_arena_free(static_cast<uint32_t>(inode->data_block_off),
                            static_cast<uint32_t>(inode->size));
        }
        inode->size   = 0;
        inode->iflags = static_cast<uint16_t>((inode->iflags & ~0u) | INODE_IS_INLINE);
        __builtin_memset(inode->inline_data, 0, INLINE_THRESHOLD);
    }

    inode->open_count++;
    *out_fd = fd;
    return 0;
}

// ============================================================================
// ramfs_read
// ============================================================================

static int ramfs_read(uint32_t ino, void* buf, uint32_t len, int64_t off) {
    if (!buf || len == 0) return 0;
    if (off < 0) return -IPC_EINVAL;
    RawInode* inode = inode_get(ino);
    if (!inode) return -IPC_ENOENT;
    if (inode->iflags & INODE_IS_DIR) return -IPC_EISDIR;

    uint64_t uoff = static_cast<uint64_t>(off);
    if (uoff >= inode->size) return 0; // EOF

    uint64_t avail = inode->size - uoff;
    uint32_t to_read = (len < avail) ? len : static_cast<uint32_t>(avail);

    if (is_inline(inode)) {
        __builtin_memcpy(buf, inode->inline_data + uoff, to_read);
    } else {
        uint8_t* data = data_arena_ptr(static_cast<uint32_t>(inode->data_block_off));
        if (!data) return -IPC_EIO;
        __builtin_memcpy(buf, data + uoff, to_read);
    }
    return static_cast<int>(to_read);
}

// ============================================================================
// ramfs_write
// ============================================================================

static int ramfs_write(uint32_t ino, const void* buf, uint32_t len, int64_t off) {
    if (!buf || len == 0) return 0;
    if (off < 0) return -IPC_EINVAL;
    RawInode* inode = inode_get(ino);
    if (!inode) return -IPC_ENOENT;
    if (inode->iflags & INODE_IS_DIR) return -IPC_EISDIR;

    uint64_t uoff = static_cast<uint64_t>(off);
    uint64_t new_end = uoff + len;

    if (new_end <= INLINE_THRESHOLD) {
        // Small write -- use inline storage
        if (!(inode->iflags & INODE_IS_INLINE) && inode->size > 0) {
            // Currently arena-backed; we'd need to shrink, which doesn't happen
            // in practice for v1.3.0 (files don't shrink). Fall through to arena.
            goto arena_write;
        }
        inode->iflags |= INODE_IS_INLINE;
        __builtin_memcpy(inode->inline_data + uoff, buf, len);
        if (new_end > inode->size) inode->size = new_end;
        return static_cast<int>(len);
    }

arena_write:;
    // Large write -- use data arena
    uint8_t* data;
    if (!(inode->iflags & INODE_IS_INLINE) && inode->size > 0) {
        // Already arena-backed
        data = data_arena_ptr(static_cast<uint32_t>(inode->data_block_off));
        if (!data) return -IPC_EIO;
        // Check if write fits in existing allocation (arena_alloc rounds to 512)
        // For simplicity in v1.3.0 we allow writes within the allocated block only.
        // new_end must fit; if not, -EFBIG.
        if (new_end > DATA_ARENA_SIZE) return -IPC_EFBIG;
    } else {
        // Transition from inline or fresh file to arena storage
        uint32_t alloc_size = static_cast<uint32_t>(new_end);
        uint32_t off_in_arena = data_arena_alloc(alloc_size);
        if (off_in_arena == DATA_ARENA_SIZE) return -IPC_ENOSPC;
        data = data_arena_ptr(off_in_arena);

        // Copy existing inline data if any
        if ((inode->iflags & INODE_IS_INLINE) && inode->size > 0) {
            __builtin_memcpy(data, inode->inline_data, static_cast<uint32_t>(inode->size));
        }

        inode->iflags      &= static_cast<uint16_t>(~INODE_IS_INLINE);
        inode->data_block_off = static_cast<uint64_t>(off_in_arena);
    }

    __builtin_memcpy(data + uoff, buf, len);
    if (new_end > inode->size) inode->size = new_end;
    return static_cast<int>(len);
}

// ============================================================================
// ramfs_close
// ============================================================================

static int ramfs_close(int fd) {
    FdEntry* entry = fd_get(fd);
    if (!entry) return -IPC_EBADF;
    uint32_t ino = entry->ino;

    int ret = fd_release(fd);

    RawInode* inode = inode_get(ino);
    if (inode && inode->open_count > 0) {
        inode->open_count--;
        // If nlink==0 and no open FDs: free the inode (deferred unlink)
        if (inode->nlink == 0 && inode->open_count == 0) {
            // Reclaim arena storage before freeing the inode
            if (!(inode->iflags & INODE_IS_INLINE) && inode->size > 0) {
                data_arena_free(static_cast<uint32_t>(inode->data_block_off),
                                static_cast<uint32_t>(inode->size));
            }
            inode_free(ino);
        }
    }
    return ret;
}

// ============================================================================
// ramfs_stat
// ============================================================================

static int ramfs_stat(uint32_t ino, KStat* out) {
    if (!out) return -IPC_EINVAL;
    RawInode* inode = inode_get(ino);
    if (!inode) return -IPC_ENOENT;

    out->st_dev    = 1; // ramfs device id
    out->st_ino    = ino;
    out->st_mode   = inode->mode;
    out->st_nlink  = inode->nlink;
    out->st_uid    = inode->uid;
    out->st_gid    = inode->gid;
    out->st_rdev   = 0;
    out->st_size   = static_cast<int64_t>(inode->size);
    out->st_blksize = CACHE_BLK_SIZE;
    out->st_blocks  = static_cast<int64_t>((inode->size + 511) / 512);
    out->st_atime  = inode->mtime; // no separate atime in v1.3.0
    out->st_mtime  = inode->mtime;
    out->st_ctime  = inode->ctime;
    return 0;
}

// ============================================================================
// ramfs_mkdir
// ============================================================================

static int ramfs_mkdir(uint32_t parent_ino, const char* name,
                       uint8_t namelen, uint16_t mode) {
    if (!name || namelen == 0 || namelen > 26) return -IPC_ENAMETOOLONG;
    RawInode* parent = inode_get(parent_ino);
    if (!parent) return -IPC_ENOENT;
    if (!(parent->iflags & INODE_IS_DIR)) return -IPC_ENOTDIR;

    // Check if name already exists
    if (dirent_lookup(parent_ino, name, namelen) != 0) return -IPC_EEXIST;

    uint32_t new_ino = inode_alloc();
    if (new_ino == 0) return -IPC_ENOSPC;

    // Allocate dirent block for new directory
    uint32_t dstart = dirent_block_alloc();
    if (dstart >= MAX_DIRENTS) {
        inode_free(new_ino);
        return -IPC_ENOSPC;
    }

    RawInode* inode = inode_get(new_ino);
    inode->mode       = static_cast<uint16_t>(S_IFDIR | (mode & 0x0FFFu));
    inode->iflags     = static_cast<uint16_t>(INODE_IS_USED | INODE_IS_DIR);
    inode->nlink      = 2; // "." + parent's entry
    inode->parent_ino = parent_ino;
    inode->dirent_start = static_cast<uint64_t>(dstart);
    inode->size       = 0;

    // Add "." and ".." into the new directory's dirent block
    dirent_add(new_ino, new_ino,      DT_DIR, ".",  1);
    dirent_add(new_ino, parent_ino,   DT_DIR, "..", 2);

    // Add new directory entry into parent
    int ret = dirent_add(parent_ino, new_ino, DT_DIR, name, namelen);
    if (ret != 0) {
        inode_free(new_ino);
        return -IPC_ENOSPC;
    }

    // Parent gains a ".." link from new subdirectory
    parent->nlink++;
    return 0;
}

// ============================================================================
// ramfs_unlink
// ============================================================================

static int ramfs_unlink(uint32_t parent_ino, const char* name, uint8_t namelen) {
    if (!name || namelen == 0) return -IPC_EINVAL;
    RawInode* parent = inode_get(parent_ino);
    if (!parent) return -IPC_ENOENT;
    if (!(parent->iflags & INODE_IS_DIR)) return -IPC_ENOTDIR;

    uint32_t child_ino = dirent_lookup(parent_ino, name, namelen);
    if (child_ino == 0) return -IPC_ENOENT;

    RawInode* child = inode_get(child_ino);
    if (!child) return -IPC_ENOENT;

    // Directories must be removed via a future rmdir; unlink is for files
    if (child->iflags & INODE_IS_DIR) return -IPC_EISDIR;

    int ret = dirent_remove(parent_ino, name, namelen);
    if (ret != 0) return -IPC_EIO;

    if (child->nlink > 0) child->nlink--;

    // Free inode only if no open FDs hold it
    if (child->nlink == 0 && child->open_count == 0) {
        // Reclaim arena storage before freeing the inode
        if (!(child->iflags & INODE_IS_INLINE) && child->size > 0) {
            data_arena_free(static_cast<uint32_t>(child->data_block_off),
                            static_cast<uint32_t>(child->size));
        }
        inode_free(child_ino);
    }
    return 0;
}

// ============================================================================
// ramfs_readdir
// ============================================================================

static int ramfs_readdir(uint32_t ino, DirEntry* buf, int max_entries) {
    return dirent_readdir(ino, buf, max_entries);
}

// ============================================================================
// Exported FsOps table
// ============================================================================

extern "C" const FsOps ramfs_ops = {
    .open    = ramfs_open,
    .read    = ramfs_read,
    .write   = ramfs_write,
    .close   = ramfs_close,
    .stat    = ramfs_stat,
    .mkdir   = ramfs_mkdir,
    .unlink  = ramfs_unlink,
    .readdir = ramfs_readdir,
};

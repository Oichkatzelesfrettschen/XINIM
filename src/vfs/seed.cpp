#include "seed.hpp"

#include "core_init.hpp"
#include "dirent.hpp"
#include "inode_table.hpp"
#include "path_walk.hpp"

#include <xinim/ipc/message_types.h>

namespace {

inline constexpr std::size_t INLINE_FILE_THRESHOLD = sizeof(RawInode{}.inline_data);

int write_seed_data(RawInode* inode, const void* data, std::size_t size) {
    if (!inode || (!data && size != 0U)) {
        return -IPC_EINVAL;
    }

    if (!(inode->iflags & INODE_IS_INLINE) && inode->size > 0) {
        data_arena_free(static_cast<uint32_t>(inode->data_block_off),
                        static_cast<uint32_t>(inode->size));
    }

    inode->size = static_cast<uint64_t>(size);
    if (size <= INLINE_FILE_THRESHOLD) {
        inode->iflags = static_cast<uint16_t>((inode->iflags | INODE_IS_INLINE) & ~INODE_IS_DIR);
        __builtin_memset(inode->inline_data, 0, INLINE_FILE_THRESHOLD);
        if (size > 0U) {
            __builtin_memcpy(inode->inline_data, data, size);
        }
        return 0;
    }

    uint32_t arena_off = data_arena_alloc(static_cast<uint32_t>(size));
    if (arena_off == DATA_ARENA_SIZE) {
        return -IPC_ENOSPC;
    }

    inode->iflags = static_cast<uint16_t>((inode->iflags & ~INODE_IS_INLINE) & ~INODE_IS_DIR);
    inode->data_block_off = arena_off;
    __builtin_memcpy(data_arena_ptr(arena_off), data, size);
    return 0;
}

std::size_t text_length(const char* text) {
    if (!text) {
        return 0U;
    }
    std::size_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

} // namespace

int vfs_seed_file(const char* path, const void* data, std::size_t size, uint16_t mode) {
    if (!vfs_core_initialized() || !path || path[0] != '/') {
        return -IPC_EINVAL;
    }

    uint32_t ino = path_walk(path);
    if (ino == 0U) {
        const char* name = nullptr;
        uint8_t namelen = 0;
        const uint32_t parent = path_walk_parent(path, &name, &namelen);
        if (parent == 0U || !name || namelen == 0U) {
            return -IPC_ENOENT;
        }

        ino = inode_alloc();
        if (ino == 0U) {
            return -IPC_ENOSPC;
        }

        RawInode* fresh = inode_get(ino);
        if (!fresh) {
            inode_free(ino);
            return -IPC_EIO;
        }

        __builtin_memset(fresh, 0, sizeof(*fresh));
        fresh->ino = ino;
        fresh->mode = mode;
        fresh->iflags = INODE_IS_USED | INODE_IS_INLINE;
        fresh->uid = 0;
        fresh->gid = 0;
        fresh->nlink = 1;
        fresh->parent_ino = parent;

        const int add_result = dirent_add(parent, ino, DT_REG, name, namelen);
        if (add_result != 0) {
            inode_free(ino);
            return add_result;
        }
    }

    RawInode* inode_ptr = inode_get(ino);
    if (!inode_ptr) {
        return -IPC_ENOENT;
    }
    if (inode_ptr->iflags & INODE_IS_DIR) {
        return -IPC_EISDIR;
    }

    inode_ptr->mode = mode;
    inode_ptr->iflags |= INODE_IS_USED;
    return write_seed_data(inode_ptr, data, size);
}

int vfs_seed_text(const char* path, const char* text, uint16_t mode) {
    if (!text) {
        return -IPC_EINVAL;
    }
    return vfs_seed_file(path, text, text_length(text), mode);
}

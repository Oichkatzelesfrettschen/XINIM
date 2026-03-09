#include "core_init.hpp"

#include "bare_vfs.hpp"
#include "buffer_cache.hpp"
#include "dirent.hpp"
#include "fd_table.hpp"
#include "inode_table.hpp"
#include "mount_table.hpp"
#include "ramfs_ops.hpp"
#include "vnode_table.hpp"

namespace {

bool g_vfs_core_initialized = false;

void create_root_dir() {
    const uint32_t ino = inode_alloc();
    const uint32_t dstart = dirent_block_alloc();

    RawInode* root = inode_get(ino);
    if (!root) {
        return;
    }

    root->mode = static_cast<uint16_t>(S_IFDIR | 0x1EDu);
    root->iflags = static_cast<uint16_t>(INODE_IS_USED | INODE_IS_DIR);
    root->nlink = 2;
    root->parent_ino = ino;
    root->dirent_start = static_cast<uint64_t>(dstart);
    root->size = 0;

    dirent_add(ino, ino, DT_DIR, ".", 1);
    dirent_add(ino, ino, DT_DIR, "..", 2);
}

void create_std_dir(const char* name, uint8_t namelen) {
    (void)ramfs_ops.mkdir(1U, name, namelen, 0x1EDu);
}

} // namespace

void vfs_core_init() {
    if (g_vfs_core_initialized) {
        return;
    }

    inode_table_init();
    vnode_table_init();
    dirent_table_init();
    fd_table_init();
    cache_init();
    create_root_dir();
    mount_table_init();

    create_std_dir("bin", 3);
    create_std_dir("dev", 3);
    create_std_dir("proc", 4);
    create_std_dir("tmp", 3);
    create_std_dir("etc", 3);

    g_vfs_core_initialized = true;
}

bool vfs_core_initialized() {
    return g_vfs_core_initialized;
}

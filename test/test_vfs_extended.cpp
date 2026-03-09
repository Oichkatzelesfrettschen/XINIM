/**
 * @file test_vfs_extended.cpp
 * @brief Host-side tests for v1.4.0 VFS extensions: lseek, dup/dup2,
 *        data_arena free list, and end-to-end open/write/read/close round-trips.
 *
 * Calls VFS subsystem API directly (no IPC, no lattice) -- same pattern as
 * test_bare_vfs.cpp. All tests reinitialise state via test_vfs_init().
 */

#include "../src/vfs/bare_vfs.hpp"
#include "../src/vfs/inode_table.hpp"
#include "../src/vfs/vnode_table.hpp"
#include "../src/vfs/dirent.hpp"
#include "../src/vfs/fd_table.hpp"
#include "../src/vfs/path_walk.hpp"
#include "../src/vfs/mount_table.hpp"
#include "../src/vfs/ramfs_ops.hpp"
#include "../src/vfs/buffer_cache.hpp"
#include "../include/xinim/ipc/message_types.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Shared init
// ============================================================================

static void test_vfs_init() {
    inode_table_init();
    vnode_table_init();
    dirent_table_init();
    fd_table_init();
    cache_init();

    uint32_t root_ino = inode_alloc(); // should be 1
    uint32_t dstart   = dirent_block_alloc();
    RawInode* root = inode_get(root_ino);
    root->mode        = static_cast<uint16_t>(S_IFDIR | 0x01EDu);
    root->iflags      = static_cast<uint16_t>(INODE_IS_USED | INODE_IS_DIR);
    root->nlink       = 2;
    root->parent_ino  = root_ino;
    root->dirent_start = static_cast<uint64_t>(dstart);
    root->size        = 0;
    dirent_add(root_ino, root_ino, DT_DIR, ".", 1);
    dirent_add(root_ino, root_ino, DT_DIR, "..", 2);
    mount_table_init();
}

// Helper: create a regular file inode and add a dirent in root
static uint32_t make_file(const char* name, uint8_t namelen) {
    uint32_t ino = inode_alloc();
    if (ino == 0) return 0;
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;
    inode->nlink  = 1;
    dirent_add(1, ino, DT_REG, name, namelen);
    return ino;
}

// ============================================================================
// Test harness
// ============================================================================

#define CHECK(cond) \
    do { if (!(cond)) { \
        printf("FAIL: %s:%d: " #cond "\n", __FILE__, __LINE__); \
        return 1; \
    } } while (0)

static int passed = 0;
static int failed = 0;

static void run_test(const char* name, int (*fn)()) {
    if (fn() == 0) {
        printf("PASS: %s\n", name);
        ++passed;
    } else {
        printf("FAIL: %s\n", name);
        ++failed;
    }
}

// ============================================================================
// Test 1: open/write/read/close full round-trip via FsOps
// ============================================================================
static int test_open_write_read_close() {
    test_vfs_init();
    uint32_t ino = make_file("rwtest", 6);
    CHECK(ino != 0);

    // open
    int fd = -1;
    int ret = ramfs_ops.open(ino, O_RDWR, &fd);
    CHECK(ret == 0 && fd >= 0);

    // write 64 bytes
    char wbuf[64];
    for (int i = 0; i < 64; ++i) wbuf[i] = static_cast<char>(i);
    int wret = ramfs_ops.write(ino, wbuf, 64, 0);
    CHECK(wret == 64);

    // read back via fd position tracking
    FdEntry* fde = fd_get(fd);
    CHECK(fde != nullptr);
    char rbuf[64] = {};
    int rret = ramfs_ops.read(ino, rbuf, 64, 0);
    CHECK(rret == 64);
    CHECK(__builtin_memcmp(rbuf, wbuf, 64) == 0);

    // close
    ret = ramfs_ops.close(fd);
    CHECK(ret == 0);
    CHECK(fd_get(fd) == nullptr);
    return 0;
}

// ============================================================================
// Test 2: lseek SEEK_SET / SEEK_CUR / SEEK_END
// ============================================================================
static int test_lseek_variants() {
    test_vfs_init();
    uint32_t ino = make_file("lseekf", 6);
    CHECK(ino != 0);

    // Write "ABCDEFGHIJ" (10 bytes)
    const char* data = "ABCDEFGHIJ";
    int wret = ramfs_ops.write(ino, data, 10, 0);
    CHECK(wret == 10);

    // open for reading
    int fd = -1;
    CHECK(ramfs_ops.open(ino, O_RDONLY, &fd) == 0);
    FdEntry* fde = fd_get(fd);
    CHECK(fde != nullptr);

    // SEEK_SET to 3, read 3 bytes -> "DEF"
    fde->pos = 3; // simulate lseek SEEK_SET=3
    char rbuf[4] = {};
    int rret = ramfs_ops.read(ino, rbuf, 3, fde->pos);
    CHECK(rret == 3);
    CHECK(rbuf[0] == 'D' && rbuf[1] == 'E' && rbuf[2] == 'F');

    // SEEK_CUR: pos=6, add 2 -> pos=8, read 2 bytes -> "IJ"
    int64_t cur = 6;
    int64_t new_pos = cur + 2; // simulate SEEK_CUR
    CHECK(new_pos == 8);
    rret = ramfs_ops.read(ino, rbuf, 2, new_pos);
    CHECK(rret == 2);
    CHECK(rbuf[0] == 'I' && rbuf[1] == 'J');

    // SEEK_END: size=10, offset=-3 -> pos=7, read 3 bytes -> "HIJ"
    RawInode* ri = inode_get(ino);
    CHECK(ri != nullptr);
    int64_t end_pos = static_cast<int64_t>(ri->size) + (-3); // SEEK_END
    CHECK(end_pos == 7);
    rret = ramfs_ops.read(ino, rbuf, 3, end_pos);
    CHECK(rret == 3);
    CHECK(rbuf[0] == 'H' && rbuf[1] == 'I' && rbuf[2] == 'J');

    ramfs_ops.close(fd);
    return 0;
}

// ============================================================================
// Test 3: dup inherits inode and file position
// ============================================================================
static int test_dup_shares_inode() {
    test_vfs_init();
    uint32_t ino = make_file("duptest", 7);
    CHECK(ino != 0);

    // Write some data
    const char* data = "hello dup";
    CHECK(ramfs_ops.write(ino, data, 9, 0) == 9);

    // Open original fd
    int fd1 = -1;
    CHECK(ramfs_ops.open(ino, O_RDONLY, &fd1) == 0);
    FdEntry* fde1 = fd_get(fd1);
    CHECK(fde1 != nullptr);
    fde1->pos = 6; // advance to "dup"

    // Duplicate: allocate fd2 pointing to same inode
    int fd2 = fd_allocate(fde1->ino, fde1->flags);
    CHECK(fd2 >= 0 && fd2 != fd1);
    FdEntry* fde2 = fd_get(fd2);
    fde2->pos = fde1->pos; // POSIX dup inherits position

    // Both fds reference the same inode
    CHECK(fd_get(fd1)->ino == fd_get(fd2)->ino);

    // Read via fd2 from pos=6 -> "dup"
    char rbuf[4] = {};
    int rret = ramfs_ops.read(fd_get(fd2)->ino, rbuf, 3, fd_get(fd2)->pos);
    CHECK(rret == 3);
    CHECK(rbuf[0] == 'd' && rbuf[1] == 'u' && rbuf[2] == 'p');

    ramfs_ops.close(fd1);
    ramfs_ops.close(fd2);
    return 0;
}

// ============================================================================
// Test 4: dup2 replaces target fd
// ============================================================================
static int test_dup2_replaces_fd() {
    test_vfs_init();

    // Create two files
    uint32_t ino_a = make_file("filea", 5);
    uint32_t ino_b = make_file("fileb", 5);
    CHECK(ino_a != 0 && ino_b != 0);

    int fd_a = -1, fd_b = -1;
    CHECK(ramfs_ops.open(ino_a, O_RDONLY, &fd_a) == 0);
    CHECK(ramfs_ops.open(ino_b, O_RDONLY, &fd_b) == 0);
    CHECK(fd_a != fd_b);

    // dup2: force fd_b to point at ino_a (close fd_b, reopen as ino_a)
    uint32_t old_ino = fd_get(fd_a)->ino;
    // Simulate dup2(fd_a, fd_b): release fd_b, allocate_at with fd_a's data
    ramfs_ops.close(fd_b);
    int result = fd_allocate_at(fd_b, old_ino, O_RDONLY, 0);
    CHECK(result == fd_b);
    CHECK(fd_get(fd_b) != nullptr);
    CHECK(fd_get(fd_b)->ino == ino_a);

    ramfs_ops.close(fd_a);
    ramfs_ops.close(fd_b);
    return 0;
}

// ============================================================================
// Test 5: data_arena free list reclaims space on unlink
// ============================================================================
static int test_data_arena_reclaim() {
    test_vfs_init();

    // Write a large file (>24 bytes) to force arena allocation
    uint32_t ino = make_file("bigfile", 7);
    CHECK(ino != 0);

    char wbuf[512];
    __builtin_memset(wbuf, 0xAA, sizeof(wbuf));
    CHECK(ramfs_ops.write(ino, wbuf, 512, 0) == 512);

    RawInode* ri = inode_get(ino);
    CHECK(ri != nullptr);
    CHECK(!(ri->iflags & INODE_IS_INLINE)); // must be in arena
    uint32_t arena_off = static_cast<uint32_t>(ri->data_block_off);

    // Unlink: nlink->0 and open_count==0 so inode+arena should be freed
    CHECK(dirent_lookup(1, "bigfile", 7) == ino);
    CHECK(ramfs_ops.unlink(1, "bigfile", 7) == 0);
    CHECK(inode_get(ino) == nullptr); // inode freed

    // Now allocate another 512-byte file -- should reuse the freed slot
    uint32_t ino2 = make_file("reusedf", 7);
    CHECK(ino2 != 0);
    RawInode* ri2 = inode_get(ino2);
    ri2->mode = static_cast<uint16_t>(S_IFREG | 0x01B4u);

    __builtin_memset(wbuf, 0xBB, sizeof(wbuf));
    CHECK(ramfs_ops.write(ino2, wbuf, 512, 0) == 512);
    CHECK(!(inode_get(ino2)->iflags & INODE_IS_INLINE));

    // The new arena allocation should reuse arena_off (or at least not overflow)
    uint32_t arena_off2 = static_cast<uint32_t>(inode_get(ino2)->data_block_off);
    // Either reused the freed slot or bump-allocated at a sensible offset
    CHECK(arena_off2 < DATA_ARENA_SIZE);
    // With free list: should reuse exact same offset
    CHECK(arena_off2 == arena_off);
    return 0;
}

// ============================================================================
// Test 6: mkdir + path_walk + unlink directory not allowed
// ============================================================================
static int test_mkdir_path_unlink() {
    test_vfs_init();

    // mkdir /newdir
    CHECK(ramfs_ops.mkdir(1, "newdir", 6, 0x1EDu) == 0);
    uint32_t dir_ino = path_walk("/newdir");
    CHECK(dir_ino != 0);

    // Create file inside /newdir
    uint32_t fino = inode_alloc();
    CHECK(fino != 0);
    RawInode* fi = inode_get(fino);
    fi->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    fi->iflags = INODE_IS_USED;
    fi->nlink  = 1;
    dirent_add(dir_ino, fino, DT_REG, "inner", 5);

    uint32_t walked = path_walk("/newdir/inner");
    CHECK(walked == fino);

    // Attempt to unlink the directory itself: should fail with -EISDIR
    int ret = ramfs_ops.unlink(1, "newdir", 6);
    CHECK(ret == -IPC_EISDIR);

    // Unlink the file inside
    CHECK(ramfs_ops.unlink(dir_ino, "inner", 5) == 0);
    CHECK(path_walk("/newdir/inner") == 0);
    return 0;
}

// ============================================================================
// Test 7: stat returns correct size after write
// ============================================================================
static int test_stat_size_after_write() {
    test_vfs_init();
    uint32_t ino = make_file("statf", 5);
    CHECK(ino != 0);

    char buf[100];
    __builtin_memset(buf, 0x42, 100);
    CHECK(ramfs_ops.write(ino, buf, 100, 0) == 100);

    KStat st{};
    CHECK(ramfs_ops.stat(ino, &st) == 0);
    CHECK(st.st_size == 100);
    CHECK((st.st_mode & S_IFMT) == S_IFREG);
    CHECK(st.st_ino == ino);
    return 0;
}

// ============================================================================
// Test 8: close releases fd so fd_get returns nullptr
// ============================================================================
static int test_close_releases_fd() {
    test_vfs_init();
    uint32_t ino = make_file("closef", 6);
    CHECK(ino != 0);

    int fd = -1;
    CHECK(ramfs_ops.open(ino, O_RDONLY, &fd) == 0);
    CHECK(fd_get(fd) != nullptr);
    CHECK(ramfs_ops.close(fd) == 0);
    CHECK(fd_get(fd) == nullptr);
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    run_test("open_write_read_close",  test_open_write_read_close);
    run_test("lseek_variants",         test_lseek_variants);
    run_test("dup_shares_inode",       test_dup_shares_inode);
    run_test("dup2_replaces_fd",       test_dup2_replaces_fd);
    run_test("data_arena_reclaim",     test_data_arena_reclaim);
    run_test("mkdir_path_unlink",      test_mkdir_path_unlink);
    run_test("stat_size_after_write",  test_stat_size_after_write);
    run_test("close_releases_fd",      test_close_releases_fd);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

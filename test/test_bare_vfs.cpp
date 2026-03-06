/**
 * @file test_bare_vfs.cpp
 * @brief Host-side unit tests for the bare-metal VFS subsystem (ADR-0009)
 *
 * Links: inode_table.cpp, dirent.cpp, path_walk.cpp, ramfs_ops.cpp,
 *        mount_table.cpp, fd_table.cpp, buffer_cache.cpp
 *
 * Each test function returns 0 on pass, non-zero on failure.
 * Tests are self-contained and call vfs_server_init() to reset state.
 * Because the VFS uses static arrays, tests that mutate state must
 * re-init before the next test.
 */

#include "../src/vfs/bare_vfs.hpp"
#include "../src/vfs/inode_table.hpp"
#include "../src/vfs/dirent.hpp"
#include "../src/vfs/fd_table.hpp"
#include "../src/vfs/path_walk.hpp"
#include "../src/vfs/mount_table.hpp"
#include "../src/vfs/ramfs_ops.hpp"
#include "../src/vfs/buffer_cache.hpp"

#include <cstdio>
#include <cstring>
#include <cassert>

// ============================================================================
// Minimal VFS init for tests (no serial, no IPC -- just the subsystems)
// ============================================================================

static void test_vfs_init() {
    inode_table_init();
    dirent_table_init();
    fd_table_init();
    cache_init();

    // Create root directory at ino=1
    uint32_t root_ino = inode_alloc(); // should be 1
    uint32_t dstart   = dirent_block_alloc();

    RawInode* root = inode_get(root_ino);
    root->mode       = static_cast<uint16_t>(S_IFDIR | 0x01EDu);
    root->iflags     = static_cast<uint16_t>(INODE_IS_USED | INODE_IS_DIR);
    root->nlink      = 2;
    root->parent_ino = root_ino;
    root->dirent_start = static_cast<uint64_t>(dstart);
    root->size       = 0;
    dirent_add(root_ino, root_ino, DT_DIR, ".", 1);
    dirent_add(root_ino, root_ino, DT_DIR, "..", 2);

    mount_table_init(); // mounts "/" -> ino=1 -> ramfs_ops
}

// ============================================================================
// Test helpers
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
// Tests
// ============================================================================

// Test 1: inode_alloc / inode_free round-trip
static int test_inode_alloc_free() {
    test_vfs_init();
    uint32_t ino = inode_alloc(); // root was 1; next should be 2
    CHECK(ino != 0);
    CHECK(inode_get(ino) != nullptr);
    inode_free(ino);
    CHECK(inode_get(ino) == nullptr); // freed slot returns nullptr
    return 0;
}

// Test 2: inode_get bounds check
static int test_inode_get_bounds() {
    test_vfs_init();
    CHECK(inode_get(0) == nullptr);             // slot 0 is reserved
    CHECK(inode_get(MAX_INODES) == nullptr);    // out of range
    CHECK(inode_get(MAX_INODES + 1) == nullptr);
    CHECK(inode_get(1) != nullptr);             // root is valid
    return 0;
}

// Test 3: mkdir creates directory inode + dirent entries
static int test_mkdir() {
    test_vfs_init();
    int ret = ramfs_ops.mkdir(1, "testdir", 7, 0x1EDu);
    CHECK(ret == 0);
    uint32_t child_ino = dirent_lookup(1, "testdir", 7);
    CHECK(child_ino != 0);
    RawInode* child = inode_get(child_ino);
    CHECK(child != nullptr);
    CHECK((child->mode & S_IFMT) == S_IFDIR);
    CHECK(child->iflags & INODE_IS_DIR);
    return 0;
}

// Test 4: write small file (<= 24 bytes), verify IS_INLINE set
static int test_write_small_inline() {
    test_vfs_init();
    // Create a file: alloc inode manually and set as regular file
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;

    const char* data = "hello"; // 5 bytes
    int ret = ramfs_ops.write(ino, data, 5, 0);
    CHECK(ret == 5);

    inode = inode_get(ino); // re-fetch (not necessary, same pointer, but verifies)
    CHECK(inode->iflags & INODE_IS_INLINE);
    CHECK(inode->size == 5);
    return 0;
}

// Test 5: write large file (> 24 bytes), verify data_block_off set, IS_INLINE clear
static int test_write_large_arena() {
    test_vfs_init();
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;

    char buf[64];
    __builtin_memset(buf, 'A', sizeof(buf));
    int ret = ramfs_ops.write(ino, buf, 64, 0);
    CHECK(ret == 64);

    inode = inode_get(ino);
    CHECK(!(inode->iflags & INODE_IS_INLINE));
    CHECK(inode->size == 64);
    return 0;
}

// Test 6: read back small file matches written data
static int test_read_small_roundtrip() {
    test_vfs_init();
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;

    const char* wdata = "hi world";
    int wret = ramfs_ops.write(ino, wdata, 8, 0);
    CHECK(wret == 8);

    char rbuf[16] = {};
    int rret = ramfs_ops.read(ino, rbuf, 8, 0);
    CHECK(rret == 8);
    CHECK(__builtin_memcmp(rbuf, wdata, 8) == 0);
    return 0;
}

// Test 7: read back large file matches written data
static int test_read_large_roundtrip() {
    test_vfs_init();
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;

    char wbuf[100];
    for (int i = 0; i < 100; ++i) wbuf[i] = static_cast<char>(i);
    int wret = ramfs_ops.write(ino, wbuf, 100, 0);
    CHECK(wret == 100);

    char rbuf[100] = {};
    int rret = ramfs_ops.read(ino, rbuf, 100, 0);
    CHECK(rret == 100);
    CHECK(__builtin_memcmp(rbuf, wbuf, 100) == 0);
    return 0;
}

// Test 8: stat returns correct size, mode, ino
static int test_stat_correct() {
    test_vfs_init();
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;
    inode->nlink  = 1;

    const char* wdata = "stat_test";
    ramfs_ops.write(ino, wdata, 9, 0);

    KStat st{};
    int ret = ramfs_ops.stat(ino, &st);
    CHECK(ret == 0);
    CHECK(st.st_ino == ino);
    CHECK(st.st_size == 9);
    CHECK((st.st_mode & S_IFMT) == S_IFREG);
    return 0;
}

// Test 9: path_walk("/") returns ino=1
static int test_path_walk_root() {
    test_vfs_init();
    uint32_t ino = path_walk("/");
    CHECK(ino == 1);
    return 0;
}

// Test 10: path_walk for a created directory returns correct ino
static int test_path_walk_dir() {
    test_vfs_init();
    int ret = ramfs_ops.mkdir(1, "foo", 3, 0x1EDu);
    CHECK(ret == 0);
    uint32_t expected = dirent_lookup(1, "foo", 3);
    CHECK(expected != 0);
    uint32_t walked = path_walk("/foo");
    CHECK(walked == expected);
    return 0;
}

// Test 11: unlink removes dirent entry, decrements nlink, frees inode if nlink==0
static int test_unlink() {
    test_vfs_init();
    // Create a file via direct inode alloc + dirent add
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* inode = inode_get(ino);
    inode->mode   = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    inode->iflags = INODE_IS_USED;
    inode->nlink  = 1;
    dirent_add(1, ino, DT_REG, "toremove", 8);

    CHECK(dirent_lookup(1, "toremove", 8) == ino);
    int ret = ramfs_ops.unlink(1, "toremove", 8);
    CHECK(ret == 0);
    CHECK(dirent_lookup(1, "toremove", 8) == 0);
    CHECK(inode_get(ino) == nullptr); // should be freed (nlink->0, open_count==0)
    return 0;
}

// Test 12: fd_allocate / fd_get / fd_release cycle
static int test_fd_lifecycle() {
    test_vfs_init();
    uint32_t ino = inode_alloc();
    CHECK(ino != 0);
    RawInode* in = inode_get(ino);
    in->mode = static_cast<uint16_t>(S_IFREG | 0x01B4u);
    in->iflags = INODE_IS_USED;

    int fd = fd_allocate(ino, O_RDONLY);
    CHECK(fd >= 0);
    FdEntry* fde = fd_get(fd);
    CHECK(fde != nullptr);
    CHECK(fde->ino == ino);
    int ret = fd_release(fd);
    CHECK(ret == 0);
    CHECK(fd_get(fd) == nullptr); // released
    return 0;
}

// Test 13: mount_resolve returns correct entry for "/" and subdirs
static int test_mount_resolve() {
    test_vfs_init();
    const MountEntry* m = mount_resolve("/");
    CHECK(m != nullptr);
    CHECK(m->root_ino == 1);
    CHECK(__builtin_memcmp(m->path, "/", 1) == 0);

    // With only "/" mounted, /bin/foo also resolves to "/"
    const MountEntry* m2 = mount_resolve("/bin/foo");
    CHECK(m2 != nullptr);
    CHECK(m2->root_ino == 1);
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    run_test("inode_alloc_free",      test_inode_alloc_free);
    run_test("inode_get_bounds",      test_inode_get_bounds);
    run_test("mkdir",                 test_mkdir);
    run_test("write_small_inline",    test_write_small_inline);
    run_test("write_large_arena",     test_write_large_arena);
    run_test("read_small_roundtrip",  test_read_small_roundtrip);
    run_test("read_large_roundtrip",  test_read_large_roundtrip);
    run_test("stat_correct",          test_stat_correct);
    run_test("path_walk_root",        test_path_walk_root);
    run_test("path_walk_dir",         test_path_walk_dir);
    run_test("unlink",                test_unlink);
    run_test("fd_lifecycle",          test_fd_lifecycle);
    run_test("mount_resolve",         test_mount_resolve);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

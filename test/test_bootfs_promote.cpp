#include "../include/xinim/boot/bootinfo.hpp"
#include "../src/kernel/bootfs.hpp"
#include "../src/vfs/bootfs_promote.hpp"
#include "../src/vfs/inode_table.hpp"
#include "../src/vfs/path_walk.hpp"
#include "../src/vfs/ramfs_ops.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::printf("FAIL: %s:%d: " #cond "\n", __FILE__, __LINE__); \
            return 1;                                                    \
        }                                                                \
    } while (0)

namespace {

    uint8_t g_xash_data[] = {'X', 'A', 'S', 'H'};
    uint8_t g_mksh_data[] = {'M', 'K', 'S', 'H'};
    char g_xash_path[] = "/bin/xash";
    char g_sh_path[] = "/bin/sh";
    char g_mksh_path[] = "/bin/mksh";
    char g_mkshrc_path[] = "/etc/mkshrc";
    char g_mkshrc_data[] = "PS1='mksh# '\n";
    char g_motd_path[] = "/etc/motd";
    char g_motd_data[] = "Welcome to XINIM\n";

    xinim::boot::BootModule g_modules[] = {
        {g_xash_data, sizeof(g_xash_data), g_xash_path},
        {g_mksh_data, sizeof(g_mksh_data), g_sh_path},
        {g_mksh_data, sizeof(g_mksh_data), g_mksh_path},
        {g_mkshrc_data, sizeof(g_mkshrc_data) - 1U, g_mkshrc_path},
        {g_motd_data, sizeof(g_motd_data) - 1U, g_motd_path},
    };

} // namespace

int main() {
    const xinim::boot::BootInfo info = {
        xinim::boot::BootProtocol::Multiboot2,    nullptr, nullptr, 0U, nullptr, 0U, g_modules,
        sizeof(g_modules) / sizeof(g_modules[0]), {},      {},
    };

    xinim::kernel::bootfs::initialize(info);
    const int seeded = vfs_promote_from_bootfs();
    CHECK(seeded >= 11);

    CHECK(path_walk("/boot/xash") == 0U);
    CHECK(path_walk("/bin/xash") != 0U);
    CHECK(path_walk("/bin/sh") != 0U);
    CHECK(path_walk("/bin/mksh") != 0U);
    CHECK(path_walk("/etc/mkshrc") != 0U);
    CHECK(path_walk("/etc/motd") != 0U);

    const xinim::kernel::bootfs::FileRecord *xash_file = xinim::kernel::bootfs::find("/bin/xash");
    const xinim::kernel::bootfs::FileRecord *sh_file = xinim::kernel::bootfs::find("/bin/sh");
    const xinim::kernel::bootfs::FileRecord *mksh_file = xinim::kernel::bootfs::find("/bin/mksh");
    CHECK(xash_file != nullptr);
    CHECK(sh_file != nullptr);
    CHECK(mksh_file != nullptr);
    CHECK(xash_file->data == g_xash_data);
    CHECK(sh_file->data == g_mksh_data);
    CHECK(mksh_file->data == g_mksh_data);

    const uint32_t motd_ino = path_walk("/etc/motd");
    CHECK(motd_ino != 0U);
    char buffer[32]{};
    const int read_count = ramfs_ops.read(motd_ino, buffer, sizeof(buffer), 0);
    CHECK(read_count == static_cast<int>(sizeof(g_motd_data) - 1U));
    CHECK(std::memcmp(buffer, g_motd_data, sizeof(g_motd_data) - 1U) == 0);

    const int temporary_fd = xinim::kernel::bootfs::open(
        "/tmp/file-operations", xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR,
        0600U);
    CHECK(temporary_fd >= 3);
    const char contents[] = "abcdef";
    CHECK(xinim::kernel::bootfs::write(temporary_fd, contents, sizeof(contents) - 1U) ==
          static_cast<int>(sizeof(contents) - 1U));
    CHECK(xinim::kernel::bootfs::truncate_fd(temporary_fd, 3U) == 0);

    xinim::kernel::bootfs::FileStatus file_status{};
    CHECK(xinim::kernel::bootfs::status_fd(temporary_fd, &file_status) == 0);
    CHECK(file_status.size == 3);
    CHECK(xinim::kernel::bootfs::chown_fd(temporary_fd, 0x12345678, 0x23456789) == 0);
    CHECK(xinim::kernel::bootfs::chown_fd(temporary_fd, -1, 77) == 0);
    CHECK(xinim::kernel::bootfs::status_fd(temporary_fd, &file_status) == 0);
    CHECK(file_status.user_id == 0x12345678U);
    CHECK(file_status.group_id == 77U);

    xinim::kernel::bootfs::UserspaceStat legacy_status{};
    CHECK(xinim::kernel::bootfs::stat_fd(temporary_fd, &legacy_status) != 0);

    constexpr uintptr_t first_lock_owner = 1U;
    constexpr uintptr_t second_lock_owner = 2U;
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, first_lock_owner, 0, 2) == 0);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, second_lock_owner, 0, 1) ==
          xinim::kernel::bootfs::kLockWouldBlock);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, first_lock_owner, 2, 0) == 0);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, second_lock_owner, 0, 1) == 0);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, first_lock_owner, 0, 1) == 0);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, second_lock_owner, 1, 2) ==
          xinim::kernel::bootfs::kLockWouldBlock);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, first_lock_owner, 1, 0) == 0);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, second_lock_owner, 1, 2) == 0);
    CHECK(xinim::kernel::bootfs::update_file_lock(temporary_fd, second_lock_owner, 2, 0) == 0);
    CHECK(xinim::kernel::bootfs::close(temporary_fd) == 0);

    CHECK(xinim::kernel::bootfs::create_symbolic_link("/tmp/file-operations",
                                                      "/tmp/absolute-link") == 0);
    CHECK(xinim::kernel::bootfs::status_path_no_follow("/tmp/absolute-link", &file_status) == 0);
    CHECK((file_status.mode & 0170000U) == 0120000U);
    CHECK(file_status.size == static_cast<int64_t>(std::strlen("/tmp/file-operations")));
    CHECK(xinim::kernel::bootfs::status_path("/tmp/absolute-link", &file_status) == 0);
    CHECK((file_status.mode & 0170000U) == 0100000U);
    CHECK(file_status.size == 3);

    char link_prefix[8] = {'?', '?', '?', '?', '?', '?', '?', '?'};
    CHECK(xinim::kernel::bootfs::read_symbolic_link("/tmp/absolute-link", link_prefix, 4U) == 4);
    CHECK(std::memcmp(link_prefix, "/tmp", 4U) == 0);
    CHECK(link_prefix[4] == '?');

    CHECK(xinim::kernel::bootfs::create_symbolic_link("file-operations", "/tmp/relative-link") ==
          0);
    const int relative_fd =
        xinim::kernel::bootfs::open("/tmp/relative-link", xinim::kernel::bootfs::O_RDONLY, 0U);
    CHECK(relative_fd >= 3);
    char relative_contents[4]{};
    CHECK(xinim::kernel::bootfs::read(relative_fd, relative_contents, 3U) == 3);
    CHECK(std::memcmp(relative_contents, "abc", 3U) == 0);
    CHECK(xinim::kernel::bootfs::close(relative_fd) == 0);

    CHECK(xinim::kernel::bootfs::create_symbolic_link("/bin", "/tmp/bin-link") == 0);
    const int intermediate_fd =
        xinim::kernel::bootfs::open("/tmp/bin-link/xash", xinim::kernel::bootfs::O_RDONLY, 0U);
    CHECK(intermediate_fd >= 3);
    CHECK(xinim::kernel::bootfs::close(intermediate_fd) == 0);

    CHECK(xinim::kernel::bootfs::create_symbolic_link("loop-b", "/tmp/loop-a") == 0);
    CHECK(xinim::kernel::bootfs::create_symbolic_link("loop-a", "/tmp/loop-b") == 0);
    CHECK(xinim::kernel::bootfs::open("/tmp/loop-a", xinim::kernel::bootfs::O_RDONLY, 0U) < 0);
    CHECK(xinim::kernel::bootfs::status_path("/tmp/loop-a", &file_status) != 0);

    CHECK(xinim::kernel::bootfs::mkdir("/tmp/tree", 0755U) == 0);
    CHECK(xinim::kernel::bootfs::mkdir("/tmp/tree/subdirectory", 0755U) == 0);
    const int tree_file = xinim::kernel::bootfs::open(
        "/tmp/tree/subdirectory/file",
        xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U);
    CHECK(tree_file >= 3);
    CHECK(xinim::kernel::bootfs::write(tree_file, "tree", 4U) == 4);
    CHECK(xinim::kernel::bootfs::close(tree_file) == 0);
    CHECK(xinim::kernel::bootfs::rename("/tmp/tree", "/tmp/moved") == 0);
    CHECK(xinim::kernel::bootfs::find("/tmp/tree") == nullptr);
    CHECK(xinim::kernel::bootfs::find("/tmp/moved/subdirectory/file") != nullptr);
    CHECK(xinim::kernel::bootfs::rmdir("/tmp/moved") != 0);
    CHECK(xinim::kernel::bootfs::unlink("/tmp/moved/subdirectory/file") == 0);
    CHECK(xinim::kernel::bootfs::rmdir("/tmp/moved/subdirectory") == 0);
    CHECK(xinim::kernel::bootfs::rmdir("/tmp/moved") == 0);

    const int retained_fd = xinim::kernel::bootfs::open(
        "/tmp/open-unlinked", xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR,
        0600U);
    CHECK(retained_fd >= 3);
    CHECK(xinim::kernel::bootfs::write(retained_fd, "retained", 8U) == 8);
    CHECK(xinim::kernel::bootfs::unlink("/tmp/open-unlinked") == 0);
    CHECK(xinim::kernel::bootfs::find("/tmp/open-unlinked") == nullptr);
    CHECK(xinim::kernel::bootfs::seek(retained_fd, 0, 0) == 0);
    char retained_contents[8]{};
    CHECK(xinim::kernel::bootfs::read(retained_fd, retained_contents,
                                      static_cast<uint32_t>(sizeof(retained_contents))) == 8);
    CHECK(std::memcmp(retained_contents, "retained", 8U) == 0);
    CHECK(xinim::kernel::bootfs::close(retained_fd) == 0);

    for (int iteration = 0; iteration < 24; ++iteration) {
        const int reused_fd = xinim::kernel::bootfs::open(
            "/tmp/reused", xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U);
        CHECK(reused_fd >= 3);
        CHECK(xinim::kernel::bootfs::close(reused_fd) == 0);
        CHECK(xinim::kernel::bootfs::unlink("/tmp/reused") == 0);
    }

    CHECK(xinim::kernel::bootfs::find("") == nullptr);
    CHECK(xinim::kernel::bootfs::open("", xinim::kernel::bootfs::O_RDONLY, 0U) < 0);
    CHECK(xinim::kernel::bootfs::find("/../../bin//./sh") == sh_file);

    CHECK(xinim::kernel::bootfs::mkdir("/tmp/canonical", 0755U) == 0);
    CHECK(xinim::kernel::bootfs::mkdir("/tmp/canonical/child", 0755U) == 0);
    const int canonical_fd = xinim::kernel::bootfs::open(
        "/tmp//canonical/child/.././file",
        xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U);
    CHECK(canonical_fd >= 3);
    CHECK(xinim::kernel::bootfs::close(canonical_fd) == 0);
    CHECK(xinim::kernel::bootfs::find("/tmp/canonical/file") != nullptr);

    CHECK(xinim::kernel::bootfs::create_symbolic_link("/tmp/canonical", "/tmp/canonical-link") ==
          0);
    CHECK(xinim::kernel::bootfs::mkdir("/tmp/canonical-link/linked-directory", 0755U) == 0);
    const int linked_parent_fd = xinim::kernel::bootfs::open(
        "/tmp/canonical-link/linked-directory/created",
        xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U);
    CHECK(linked_parent_fd >= 3);
    CHECK(xinim::kernel::bootfs::close(linked_parent_fd) == 0);
    CHECK(xinim::kernel::bootfs::rename("/tmp/canonical-link/linked-directory/created",
                                        "/tmp/canonical-link/linked-directory/renamed") == 0);
    CHECK(xinim::kernel::bootfs::access("/tmp/canonical-link/linked-directory/renamed") == 0);
    CHECK(xinim::kernel::bootfs::find("/tmp/canonical/linked-directory/renamed") != nullptr);

    CHECK(xinim::kernel::bootfs::unlink("/tmp/canonical-link/linked-directory/renamed") == 0);
    CHECK(xinim::kernel::bootfs::rmdir("/tmp/canonical-link/linked-directory") == 0);
    CHECK(xinim::kernel::bootfs::unlink("/tmp/canonical-link") == 0);
    CHECK(xinim::kernel::bootfs::unlink("/tmp/canonical/file") == 0);
    CHECK(xinim::kernel::bootfs::rmdir("/tmp/canonical/child") == 0);
    CHECK(xinim::kernel::bootfs::rmdir("/tmp/canonical") == 0);

    xinim::kernel::bootfs::initialize(info);
    CHECK(xinim::kernel::bootfs::open("/tmp/absent", xinim::kernel::bootfs::O_RDONLY, 0U) ==
          -ENOENT);
    CHECK(xinim::kernel::bootfs::open("/bin/xash",
                                      xinim::kernel::bootfs::O_CREAT |
                                          xinim::kernel::bootfs::O_EXCL |
                                          xinim::kernel::bootfs::O_RDONLY,
                                      0600U) == -EEXIST);
    CHECK(xinim::kernel::bootfs::open("/bin/xash", xinim::kernel::bootfs::O_WRONLY, 0U) == -EROFS);
    CHECK(xinim::kernel::bootfs::open("/bin", xinim::kernel::bootfs::O_WRONLY, 0U) == -EISDIR);
    CHECK(xinim::kernel::bootfs::open(
              "/read-only/new", xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR,
              0600U) == -EROFS);
    CHECK(xinim::kernel::bootfs::open(
              "/tmp/absent/new", xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR,
              0600U) == -ENOENT);

    static_assert(xinim::kernel::bootfs::kMaximumDynamicNodes > 16U);
    static_assert(xinim::kernel::bootfs::kMaximumFileRecords >=
                  xinim::kernel::bootfs::kStaticFileRecordCapacity +
                      xinim::kernel::bootfs::kMaximumDynamicNodes +
                      xinim::kernel::bootfs::kMaximumSymbolicLinks);
    char capacity_path[64]{};
    for (size_t node_index = 0U; node_index < xinim::kernel::bootfs::kMaximumDynamicNodes;
         ++node_index) {
        const int path_length =
            std::snprintf(capacity_path, sizeof(capacity_path), "/tmp/capacity-%zu", node_index);
        CHECK(path_length > 0);
        const int capacity_fd = xinim::kernel::bootfs::open(
            capacity_path, xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U);
        CHECK(capacity_fd >= 3);
        CHECK(xinim::kernel::bootfs::close(capacity_fd) == 0);
        CHECK(xinim::kernel::bootfs::find(capacity_path) != nullptr);
    }
    CHECK(xinim::kernel::bootfs::open(
              "/tmp/capacity-overflow",
              xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U) == -ENOSPC);

    CHECK(xinim::kernel::bootfs::unlink("/tmp/capacity-17") == 0);
    const int replacement_fd = xinim::kernel::bootfs::open(
        "/tmp/capacity-replacement", xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR,
        0600U);
    CHECK(replacement_fd >= 3);
    CHECK(xinim::kernel::bootfs::close(replacement_fd) == 0);
    CHECK(xinim::kernel::bootfs::open(
              "/tmp/capacity-still-full",
              xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U) == -ENOSPC);

    int backend_descriptors[xinim::kernel::bootfs::kMaximumBackendUserOpenFiles]{};
    for (size_t descriptor_index = 0U;
         descriptor_index < xinim::kernel::bootfs::kMaximumBackendUserOpenFiles;
         ++descriptor_index) {
        backend_descriptors[descriptor_index] =
            xinim::kernel::bootfs::open("/bin/xash", xinim::kernel::bootfs::O_RDONLY, 0U);
        CHECK(backend_descriptors[descriptor_index] >= 3);
    }
    CHECK(xinim::kernel::bootfs::open("/bin/xash", xinim::kernel::bootfs::O_RDONLY, 0U) == -EMFILE);
    for (const int backend_descriptor : backend_descriptors) {
        CHECK(xinim::kernel::bootfs::close(backend_descriptor) == 0);
    }

    for (size_t node_index = 0U; node_index < xinim::kernel::bootfs::kMaximumDynamicNodes;
         ++node_index) {
        if (node_index == 17U) {
            continue;
        }
        const int path_length =
            std::snprintf(capacity_path, sizeof(capacity_path), "/tmp/capacity-%zu", node_index);
        CHECK(path_length > 0);
        CHECK(xinim::kernel::bootfs::unlink(capacity_path) == 0);
    }
    CHECK(xinim::kernel::bootfs::unlink("/tmp/capacity-replacement") == 0);
    const int post_cleanup_fd = xinim::kernel::bootfs::open(
        "/tmp/post-capacity-cleanup",
        xinim::kernel::bootfs::O_CREAT | xinim::kernel::bootfs::O_RDWR, 0600U);
    CHECK(post_cleanup_fd >= 3);
    CHECK(xinim::kernel::bootfs::close(post_cleanup_fd) == 0);
    CHECK(xinim::kernel::bootfs::unlink("/tmp/post-capacity-cleanup") == 0);

    std::printf("PASS: bootfs promotion\n");
    return 0;
}

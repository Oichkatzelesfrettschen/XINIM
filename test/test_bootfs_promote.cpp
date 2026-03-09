#include "../include/xinim/boot/bootinfo.hpp"
#include "../src/kernel/bootfs.hpp"
#include "../src/vfs/bootfs_promote.hpp"
#include "../src/vfs/inode_table.hpp"
#include "../src/vfs/path_walk.hpp"
#include "../src/vfs/ramfs_ops.hpp"

#include <cstdio>
#include <cstring>

#define CHECK(cond) \
    do { if (!(cond)) { \
        std::printf("FAIL: %s:%d: " #cond "\n", __FILE__, __LINE__); \
        return 1; \
    } } while (0)

namespace {

uint8_t g_shell_data[] = {'X', 'A', 'S', 'H'};
char g_shell_path[] = "/boot/xash";
char g_motd_path[] = "/etc/motd";
char g_motd_data[] = "Welcome to xash\n";

xinim::boot::BootModule g_modules[] = {
    {g_shell_data, sizeof(g_shell_data), g_shell_path},
    {g_motd_data, sizeof(g_motd_data) - 1U, g_motd_path},
};

} // namespace

int main() {
    const xinim::boot::BootInfo info = {
        xinim::boot::BootProtocol::Multiboot2,
        nullptr,
        nullptr,
        0U,
        nullptr,
        0U,
        g_modules,
        sizeof(g_modules) / sizeof(g_modules[0]),
        {},
        {},
    };

    xinim::kernel::bootfs::initialize(info);
    const int seeded = vfs_promote_from_bootfs();
    CHECK(seeded >= 5);

    CHECK(path_walk("/boot/xash") != 0U);
    CHECK(path_walk("/bin/xash") != 0U);
    CHECK(path_walk("/bin/sh") != 0U);
    CHECK(path_walk("/etc/motd") != 0U);

    const uint32_t motd_ino = path_walk("/etc/motd");
    CHECK(motd_ino != 0U);
    char buffer[32]{};
    const int read_count = ramfs_ops.read(motd_ino, buffer, sizeof(buffer), 0);
    CHECK(read_count == static_cast<int>(sizeof(g_motd_data) - 1U));
    CHECK(std::memcmp(buffer, g_motd_data, sizeof(g_motd_data) - 1U) == 0);

    std::printf("PASS: bootfs promotion\n");
    return 0;
}

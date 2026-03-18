#include "devfs.hpp"

#include "console.hpp"
#include "tty.hpp"

namespace xinim::i486::devfs {
namespace {

using UserspaceStat = ::xinim::userland::UserspaceStat;

bool str_eq(const char* a, const char* b) noexcept {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return false;
        ++a; ++b;
    }
    return *a == *b;
}

// Device IDs for tracking which device an fd refers to
enum class DevId : uint8_t {
    None = 0,
    Null = 1,
    Zero = 2,
    Tty = 3,
};

constexpr int kMaxDevFds = 8;
struct DevFd {
    bool in_use;
    DevId device;
};
DevFd g_dev_fds[kMaxDevFds]{};

int devfs_open(const char* path, uint32_t flags, uint32_t mode) noexcept {
    (void)flags;
    (void)mode;
    if (path == nullptr) return -1;

    // Strip leading /dev or just leading /
    const char* name = path;
    if (name[0] == '/') ++name;

    DevId dev = DevId::None;
    if (str_eq(name, "null")) {
        dev = DevId::Null;
    } else if (str_eq(name, "zero")) {
        dev = DevId::Zero;
    } else if (str_eq(name, "tty") || str_eq(name, "console")) {
        dev = DevId::Tty;
    } else {
        return -1;
    }

    for (int i = 0; i < kMaxDevFds; ++i) {
        if (!g_dev_fds[i].in_use) {
            g_dev_fds[i].in_use = true;
            g_dev_fds[i].device = dev;
            return i; // Return devfs-local slot
        }
    }
    return -1;
}

int devfs_read(int slot, void* buf, uint32_t count) noexcept {
    if (slot < 0 || slot >= kMaxDevFds || !g_dev_fds[slot].in_use) return -1;
    auto* dst = static_cast<uint8_t*>(buf);
    switch (g_dev_fds[slot].device) {
    case DevId::Null:
        return 0; // EOF
    case DevId::Zero:
        for (uint32_t i = 0U; i < count; ++i) dst[i] = 0U;
        return static_cast<int>(count);
    case DevId::Tty: {
        const int result = tty::read(buf, count);
        return result;
    }
    default:
        return -1;
    }
}

int devfs_write(int slot, const void* buf, uint32_t count) noexcept {
    if (slot < 0 || slot >= kMaxDevFds || !g_dev_fds[slot].in_use) return -1;
    switch (g_dev_fds[slot].device) {
    case DevId::Null:
        return static_cast<int>(count); // Discard
    case DevId::Zero:
        return static_cast<int>(count); // Discard
    case DevId::Tty: {
        const auto* src = static_cast<const char*>(buf);
        for (uint32_t i = 0U; i < count; ++i) {
            console::tty_write_char(src[i]);
        }
        return static_cast<int>(count);
    }
    default:
        return -1;
    }
}

int devfs_close(int slot) noexcept {
    if (slot < 0 || slot >= kMaxDevFds) return -1;
    g_dev_fds[slot].in_use = false;
    g_dev_fds[slot].device = DevId::None;
    return 0;
}

int devfs_stat(const char* path, UserspaceStat* buf) noexcept {
    if (path == nullptr || buf == nullptr) return -1;
    const char* name = path;
    if (name[0] == '/') ++name;

    *buf = {};
    buf->st_dev = 2U; // devfs device id
    buf->st_ino = 1U;
    buf->st_nlink = 1U;
    buf->st_blksize = 512U;

    if (str_eq(name, "null") || str_eq(name, "zero")) {
        buf->st_mode = 0020666U; // char device, rw-rw-rw-
        return 0;
    }
    if (str_eq(name, "tty") || str_eq(name, "console")) {
        buf->st_mode = 0020620U; // char device, rw--w----
        return 0;
    }
    // /dev itself
    if (name[0] == '\0' || str_eq(name, ".")) {
        buf->st_mode = 0040755U; // directory
        buf->st_nlink = 2U;
        return 0;
    }
    return -1;
}

int devfs_access(const char* path) noexcept {
    if (path == nullptr) return -1;
    const char* name = path;
    if (name[0] == '/') ++name;
    if (str_eq(name, "null") || str_eq(name, "zero") ||
        str_eq(name, "tty") || str_eq(name, "console") ||
        name[0] == '\0') {
        return 0;
    }
    return -1;
}

vfs::VfsOps g_devfs_ops = {
    devfs_open,
    devfs_read,
    devfs_write,
    devfs_close,
    devfs_stat,
    nullptr, // mkdir
    nullptr, // unlink
    nullptr, // rename
    nullptr, // readdir
    nullptr, // chmod
    nullptr, // symlink
    nullptr, // readlink
    nullptr, // rmdir
    devfs_access,
    nullptr, // truncate
};

} // namespace

vfs::VfsOps* ops() noexcept {
    return &g_devfs_ops;
}

} // namespace xinim::i486::devfs

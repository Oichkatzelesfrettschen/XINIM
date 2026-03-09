#include "bootfs_promote.hpp"

#include "../kernel/bootfs.hpp"

#include "core_init.hpp"
#include "inode_table.hpp"
#include "path_walk.hpp"
#include "ramfs_ops.hpp"
#include "seed.hpp"

namespace {

std::size_t path_length(const char* text) {
    std::size_t length = 0U;
    if (!text) {
        return 0U;
    }
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

int ensure_directory_path(const char* path) {
    if (!path || path[0] != '/') {
        return -1;
    }
    if (path[1] == '\0') {
        return 0;
    }
    if (path_walk(path) != 0U) {
        return 0;
    }

    const char* name = nullptr;
    uint8_t namelen = 0;
    const uint32_t parent = path_walk_parent(path, &name, &namelen);
    if (parent == 0U || !name || namelen == 0U) {
        return -1;
    }
    return ramfs_ops.mkdir(parent, name, namelen, MODE_DIR_DEFAULT);
}

int ensure_parent_directories(const char* path) {
    if (!path || path[0] != '/') {
        return -1;
    }

    char normalized[64]{};
    const std::size_t length = path_length(path);
    if (length >= sizeof(normalized)) {
        return -1;
    }
    __builtin_memcpy(normalized, path, length + 1U);

    for (std::size_t index = 1U; index < length; ++index) {
        if (normalized[index] != '/') {
            continue;
        }
        normalized[index] = '\0';
        if (ensure_directory_path(normalized) != 0) {
            return -1;
        }
        normalized[index] = '/';
    }
    return 0;
}

bool seed_one(const xinim::kernel::bootfs::FileRecord& entry, void* context) {
    int& seeded = *static_cast<int*>(context);
    if (!entry.path || entry.path[0] != '/') {
        return true;
    }

    if (entry.is_directory) {
        if (ensure_directory_path(entry.path) == 0) {
            ++seeded;
        }
        return true;
    }

    if (ensure_parent_directories(entry.path) != 0) {
        return true;
    }

    const uint16_t mode = static_cast<uint16_t>(
        (entry.executable ? (S_IFREG | 0x01EDu) : MODE_FILE_DEFAULT));
    if (vfs_seed_file(entry.path, entry.data, entry.size, mode) == 0) {
        ++seeded;
    }
    return true;
}

} // namespace

int vfs_promote_from_bootfs() {
    vfs_core_init();
    int seeded = 0;
    xinim::kernel::bootfs::for_each_entry(seed_one, &seeded);
    return seeded;
}

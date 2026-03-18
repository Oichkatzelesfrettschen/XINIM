#include "vfs.hpp"

namespace xinim::i486::vfs {
namespace {

MountEntry g_mounts[kMaxMounts]{};

uint32_t str_len(const char* s) noexcept {
    if (s == nullptr) return 0U;
    uint32_t n = 0U;
    while (s[n] != '\0') ++n;
    return n;
}

void str_copy(char* dst, uint32_t cap, const char* src) noexcept {
    uint32_t i = 0U;
    while (i + 1U < cap && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

bool prefix_match(const char* path, const char* prefix, uint32_t prefix_len) noexcept {
    for (uint32_t i = 0U; i < prefix_len; ++i) {
        if (path[i] != prefix[i]) return false;
    }
    // After the prefix, path must end or have a '/' separator.
    // Special case: prefix "/" matches everything.
    if (prefix_len == 1U && prefix[0] == '/') return true;
    const char next = path[prefix_len];
    return next == '\0' || next == '/';
}

} // namespace

void initialize() noexcept {
    for (auto& m : g_mounts) {
        m.in_use = false;
    }
}

bool mount(const char* prefix, VfsOps* ops) noexcept {
    if (prefix == nullptr || ops == nullptr) return false;
    for (auto& m : g_mounts) {
        if (!m.in_use) {
            m.in_use = true;
            str_copy(m.prefix, sizeof(m.prefix), prefix);
            m.prefix_len = str_len(prefix);
            m.ops = ops;
            return true;
        }
    }
    return false;
}

VfsOps* resolve(const char* path, const char** relative_path) noexcept {
    if (path == nullptr) return nullptr;

    // Longest-prefix match: find the mount with the longest matching prefix
    MountEntry* best = nullptr;
    for (auto& m : g_mounts) {
        if (!m.in_use) continue;
        if (!prefix_match(path, m.prefix, m.prefix_len)) continue;
        if (best == nullptr || m.prefix_len > best->prefix_len) {
            best = &m;
        }
    }

    if (best == nullptr) return nullptr;

    if (relative_path != nullptr) {
        if (best->prefix_len == 1U && best->prefix[0] == '/') {
            // Root mount: relative path is the full path
            *relative_path = path;
        } else {
            // Strip the prefix; if nothing remains, use "/"
            const char* rest = path + best->prefix_len;
            *relative_path = (rest[0] == '\0') ? "/" : rest;
        }
    }

    return best->ops;
}

int mount_index(const char* path) noexcept {
    if (path == nullptr) return -1;
    int best_index = -1;
    uint32_t best_len = 0U;
    for (int i = 0; i < kMaxMounts; ++i) {
        if (!g_mounts[i].in_use) continue;
        if (!prefix_match(path, g_mounts[i].prefix, g_mounts[i].prefix_len)) continue;
        if (g_mounts[i].prefix_len > best_len) {
            best_len = g_mounts[i].prefix_len;
            best_index = i;
        }
    }
    return best_index;
}

} // namespace xinim::i486::vfs

#include "bootfs.hpp"

namespace xinim::kernel::bootfs {
namespace {

constexpr size_t kMaxFiles = 32U;
constexpr size_t kMaxDynamicFiles = 16U;
constexpr size_t kMaxOpenFiles = 8U;
constexpr size_t kMaxPathLength = 64U;
constexpr size_t kPathCacheEntries = 16U;
constexpr size_t kTmpPathLength = 64U;
constexpr size_t kTmpFileCapacity = 65536U;
constexpr int kFirstFileDescriptor = 3;

constexpr uint32_t kFileFlagO_RDONLY = 0x0000U;
constexpr uint32_t kFileFlagO_WRONLY = 0x0001U;
constexpr uint32_t kFileFlagO_RDWR = 0x0002U;
constexpr uint32_t kFileFlagO_APPEND = 0x0008U;
constexpr uint32_t kFileFlagO_CREAT = 0x0040U;
constexpr uint32_t kFileFlagO_TRUNC = 0x0200U;
constexpr uint32_t kFileFlagO_EXCL = 0x0080U;
constexpr uint32_t kFileFlagO_ACCMODE = 0x0003U;

struct OpenFile {
    bool in_use;
    FileRecord* file;
    uint32_t offset;
    uint32_t access;
    bool append_mode;
};

struct RamFile {
    bool in_use;
    char path[kTmpPathLength];
    uint8_t data[kTmpFileCapacity];
    uint32_t size;
    bool executable;
};

struct PathCacheEntry {
    bool valid;
    bool negative;
    char path[kMaxPathLength];
    const FileRecord* file;
};

FileRecord g_files[kMaxFiles]{};
OpenFile g_open_files[kMaxOpenFiles]{};
RamFile g_tmp_files[kMaxDynamicFiles]{};
PathCacheEntry g_path_cache[kPathCacheEntries]{};
uint32_t g_file_count = 0U;

[[nodiscard]] bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    uint32_t length = 0U;
    if (text == nullptr) {
        return 0U;
    }
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

uint32_t hash_path(const char* path) noexcept {
    uint32_t hash = 2166136261u;
    if (path == nullptr) {
        return 0U;
    }
    for (uint32_t index = 0U; path[index] != '\0'; ++index) {
        hash ^= static_cast<uint8_t>(path[index]);
        hash *= 16777619u;
    }
    return hash;
}

void reset_path_cache() noexcept {
    for (size_t index = 0U; index < kPathCacheEntries; ++index) {
        g_path_cache[index] = {false, false, {}, nullptr};
    }
}

PathCacheEntry* path_cache_slot(const char* path) noexcept {
    return &g_path_cache[hash_path(path) % kPathCacheEntries];
}

const FileRecord* cache_lookup(const char* path, bool& hit) noexcept {
    hit = false;
    if (path == nullptr) {
        return nullptr;
    }
    PathCacheEntry* entry = path_cache_slot(path);
    if (!entry->valid || !string_equals(entry->path, path)) {
        return nullptr;
    }
    hit = true;
    return entry->negative ? nullptr : entry->file;
}

void cache_store(const char* path, const FileRecord* file) noexcept {
    if (path == nullptr) {
        return;
    }
    PathCacheEntry* entry = path_cache_slot(path);
    entry->valid = true;
    entry->negative = (file == nullptr);
    entry->file = file;
    for (size_t index = 0U; index < kMaxPathLength; ++index) {
        entry->path[index] = path[index];
        if (path[index] == '\0') {
            break;
        }
    }
}

bool normalize_path(const char* input, char* output, uint32_t capacity) noexcept {
    if (input == nullptr || output == nullptr || capacity == 0U) {
        return false;
    }
    uint32_t index = 0U;
    if (input[0] == '\0') {
        if (capacity < 2U) {
            return false;
        }
        output[0] = '/';
        output[1] = '\0';
        return true;
    }

    if (input[0] == '/') {
        output[index] = '/';
        ++index;
        while (input[index] != '\0') {
            if (index + 1U >= capacity) {
                return false;
            }
            output[index] = input[index];
            ++index;
        }
        output[index] = '\0';
    } else {
        if (capacity < 2U) {
            return false;
        }
        output[0] = '/';
        output[1] = '\0';
        uint32_t out_len = 1U;
        uint32_t in_len = string_length(input);
        if (out_len + in_len + 1U >= capacity) {
            return false;
        }
        index = 0U;
        while (input[index] != '\0') {
            output[out_len] = input[index];
            ++out_len;
            ++index;
        }
        output[out_len] = '\0';
            index = out_len;
    }
    while (index > 1U && output[index - 1U] == '/') {
        --index;
    }
    output[index] = '\0';

    return true;
}

bool starts_with(const char* text, const char* prefix) noexcept {
    if (text == nullptr || prefix == nullptr) {
        return false;
    }
    while (*prefix != '\0') {
        if (*text != *prefix) {
            return false;
        }
        ++text;
        ++prefix;
    }
    return true;
}

[[nodiscard]] bool is_tmp_path(const char* path) noexcept {
    if (path == nullptr) {
        return false;
    }
    if (string_equals(path, "/tmp")) {
        return true;
    }
    return starts_with(path, "/tmp/");
}

[[nodiscard]] uint32_t child_name_length(const char* path,
                                        const char* child) noexcept {
    if (path == nullptr || child == nullptr) {
        return 0U;
    }
    if (string_equals(path, "/")) {
        if (!starts_with(child, "/")) {
            return 0U;
        }
        if (string_length(child) <= 1U) {
            return 0U;
        }
        const char* cursor = child + 1U;
        while (*cursor != '\0') {
            if (*cursor == '/') {
                return 0U;
            }
            ++cursor;
        }
        return string_length(child) - 1U;
    }

    uint32_t dir_len = string_length(path);
    if (dir_len == 0U || path[dir_len - 1U] == '/') {
        return 0U;
    }
    if (!starts_with(child, path)) {
        return 0U;
    }
    if (child[dir_len] != '/') {
        return 0U;
    }
    const char* base = child + dir_len + 1U;
    if (*base == '\0') {
        return 0U;
    }
    const char* rest = base;
    while (*rest != '\0') {
        if (*rest == '/') {
            return 0U;
        }
        ++rest;
    }
    return string_length(base);
}

[[nodiscard]] bool copy_child_name(
    const char* path,
    const char* child,
    char* output,
    uint32_t index,
    uint32_t capacity,
    uint32_t& next_index) noexcept {
    const uint32_t name_length = child_name_length(path, child);
    if (name_length == 0U || output == nullptr) {
        return false;
    }
    const char* base = child;
    if (!string_equals(path, "/")) {
        base = child + string_length(path) + 1U;
    } else {
        ++base;
    }
    if (name_length + 1U >= (capacity - index)) {
        return false;
    }
    for (uint32_t cursor = 0U; cursor < name_length; ++cursor) {
        output[index] = base[cursor];
        ++index;
    }
    output[index] = '\0';
    next_index = index;
    return true;
}

[[nodiscard]] bool write_directory_entry(char* destination,
                                        uint32_t capacity,
                                        uint32_t& index,
                                        const char* entry_name,
                                        bool directory) noexcept {
    if (entry_name == nullptr) {
        return false;
    }
    uint32_t name_length = string_length(entry_name);
    uint32_t extra = 1U;
    if (directory) {
        extra += 1U;
    }
    if ((name_length + extra) >= (capacity - index)) {
        return false;
    }
    for (uint32_t cursor = 0U; cursor < name_length; ++cursor) {
        destination[index] = entry_name[cursor];
        ++index;
    }
    if (directory) {
        destination[index] = '/';
        ++index;
    }
    destination[index] = '\n';
    ++index;
    destination[index] = '\0';
    return true;
}

[[nodiscard]] uint32_t build_directory_listing(
    const char* directory_path,
    char* buffer,
    uint32_t capacity) noexcept {
    if (directory_path == nullptr || buffer == nullptr || capacity == 0U) {
        return 0U;
    }
    uint32_t index = 0U;
    buffer[0] = '\0';

    const char* path = directory_path;
    if (!is_directory(path)) {
        return 0U;
    }

    for (uint32_t entry = 0U; entry < g_file_count; ++entry) {
        const char* candidate = g_files[entry].path;
        if (candidate == nullptr) {
            continue;
        }
        char child_name[kTmpPathLength]{};
        uint32_t child_index = 0U;
        if (!copy_child_name(path, candidate, child_name, 0U, sizeof(child_name), child_index)) {
            continue;
        }
        bool is_directory_entry = g_files[entry].is_directory;
        if (!write_directory_entry(
                buffer,
                capacity,
                index,
                child_name,
                is_directory_entry)) {
            break;
        }
    }
    return index;
}

void reset() noexcept {
    g_file_count = 0U;
    reset_path_cache();
    for (size_t index = 0U; index < kMaxFiles; ++index) {
        g_files[index] = {
            nullptr,
            nullptr,
            0U,
            0U,
            false,
            false,
            false,
        };
    }
    for (size_t index = 0U; index < kMaxOpenFiles; ++index) {
        g_open_files[index] = {false, nullptr, 0U, kFileFlagO_RDONLY, false};
    }
    for (size_t index = 0U; index < kMaxDynamicFiles; ++index) {
        g_tmp_files[index] = {};
    }
}

void add_directory(const char* path) noexcept {
    if (path == nullptr || g_file_count >= kMaxFiles) {
        return;
    }

    reset_path_cache();

    auto& file = g_files[g_file_count];
    file.path = path;
    file.data = nullptr;
    file.size = 0U;
    file.capacity = 0U;
    file.read_only = true;
    file.executable = false;
    file.is_directory = true;
    ++g_file_count;
}

FileRecord* add_file(
    const char* path,
    uint8_t* data,
    uint32_t size,
    bool executable) noexcept {
    if (path == nullptr || data == nullptr || size == 0U || g_file_count >= kMaxFiles) {
        return nullptr;
    }

    reset_path_cache();

    auto& file = g_files[g_file_count];
    file.path = path;
    file.data = data;
    file.size = size;
    file.capacity = size;
    file.read_only = true;
    file.executable = executable;
    file.is_directory = false;
    ++g_file_count;
    return &file;
}

FileRecord* create_tmp_file(const char* path) noexcept {
    if (!is_tmp_path(path) || g_file_count >= kMaxFiles) {
        return nullptr;
    }

    for (size_t slot = 0U; slot < kMaxDynamicFiles; ++slot) {
        if (!g_tmp_files[slot].in_use) {
            continue;
        }
        if (string_equals(g_tmp_files[slot].path, path)) {
            return nullptr;
        }
    }

    for (size_t slot = 0U; slot < kMaxDynamicFiles; ++slot) {
        if (g_tmp_files[slot].in_use) {
            continue;
        }

        uint32_t path_length = string_length(path);
        if (path_length + 1U >= kTmpPathLength) {
            return nullptr;
        }

        g_tmp_files[slot].in_use = true;
        for (uint32_t index = 0U; index <= path_length; ++index) {
            g_tmp_files[slot].path[index] = path[index];
        }
        g_tmp_files[slot].size = 0U;
        g_tmp_files[slot].executable = false;

        uint32_t remaining = kTmpFileCapacity;
        reset_path_cache();
        auto& file = g_files[g_file_count];
        file.path = g_tmp_files[slot].path;
        file.data = g_tmp_files[slot].data;
        file.size = 0U;
        file.capacity = remaining;
        file.read_only = false;
        file.executable = false;
        file.is_directory = false;
        ++g_file_count;
        return &file;
    }
    return nullptr;
}

FileRecord* get_tmp_file(const char* path) noexcept {
    if (!is_tmp_path(path)) {
        return nullptr;
    }
    for (uint32_t index = 0U; index < kMaxDynamicFiles; ++index) {
        if (g_tmp_files[index].in_use && string_equals(g_tmp_files[index].path, path)) {
            for (uint32_t entry = 0U; entry < g_file_count; ++entry) {
                if (g_files[entry].path != nullptr &&
                    string_equals(g_files[entry].path, g_tmp_files[index].path)) {
                    return &g_files[entry];
                }
            }
        }
    }
    return nullptr;
}

} // namespace

void initialize(const xinim::boot::BootInfo& info) noexcept {
    reset();

    add_directory("/");
    add_directory("/bin");
    add_directory("/boot");
    add_directory("/etc");
    add_directory("/tmp");

    for (size_t index = 0U; index < info.modules_count; ++index) {
        const xinim::boot::BootModule& module = info.modules[index];
        if (!module.valid()) {
            continue;
        }

        auto* data = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(module.address));
        const uint32_t size = static_cast<uint32_t>(module.size);
        FileRecord* file = add_file(module.string, data, size, starts_with(module.string, "/bin/"));
        if (file != nullptr && string_equals(module.string, "/bin/xash")) {
            FileRecord* sh_file = add_file("/bin/sh", data, size, true);
            (void)sh_file;
        } else if (file != nullptr && string_equals(module.string, "/boot/xash")) {
            FileRecord* xash_file = add_file("/bin/xash", data, size, true);
            FileRecord* sh_file = add_file("/bin/sh", data, size, true);
            (void)xash_file;
            (void)sh_file;
        }
    }
}

const FileRecord* find(const char* path) noexcept {
    if (path == nullptr) {
        return nullptr;
    }

    char normalized[kMaxPathLength]{};
    if (!normalize_path(path, normalized, static_cast<uint32_t>(sizeof(normalized)))) {
        return nullptr;
    }

    bool cache_hit = false;
    const FileRecord* cached = cache_lookup(normalized, cache_hit);
    if (cache_hit) {
        return cached;
    }

    for (size_t index = 0U; index < g_file_count; ++index) {
        if (string_equals(g_files[index].path, normalized)) {
            cache_store(normalized, &g_files[index]);
            return &g_files[index];
        }
    }
    const FileRecord* file = get_tmp_file(normalized);
    cache_store(normalized, file);
    return file;
}

void for_each_entry(VisitCallback callback, void* context) noexcept {
    if (callback == nullptr) {
        return;
    }
    for (uint32_t index = 0U; index < g_file_count; ++index) {
        if (g_files[index].path == nullptr) {
            continue;
        }
        if (!callback(g_files[index], context)) {
            break;
        }
    }
}

bool is_directory(const char* path) noexcept {
    const FileRecord* file = find(path);
    return file != nullptr && file->is_directory;
}

int open(const char* path, uint32_t flags, uint32_t) noexcept {
    char normalized[kMaxPathLength]{};
    if (!normalize_path(path, normalized, static_cast<uint32_t>(sizeof(normalized)))) {
        return -1;
    }

    FileRecord* file = const_cast<FileRecord*>(find(normalized));
    const bool existed = file != nullptr;
    if (file == nullptr) {
        if ((flags & kFileFlagO_CREAT) == 0U) {
            return -1;
        }
        if (!is_tmp_path(normalized)) {
            return -1;
        }
        file = create_tmp_file(normalized);
        if (file == nullptr) {
            return -1;
        }
    }

    if ((flags & kFileFlagO_EXCL) != 0U && (flags & kFileFlagO_CREAT) != 0U &&
        existed) {
        return -1;
    }

    if (file->is_directory) {
        if ((flags & kFileFlagO_ACCMODE) != kFileFlagO_RDONLY) {
            return -1;
        }
        for (size_t index = 0U; index < kMaxOpenFiles; ++index) {
            if (!g_open_files[index].in_use) {
                g_open_files[index] = {true, file, 0U, kFileFlagO_RDONLY, false};
                return static_cast<int>(kFirstFileDescriptor + index);
            }
        }
        return -1;
    }
    if (file->read_only &&
        ((flags & kFileFlagO_ACCMODE) == kFileFlagO_WRONLY ||
         (flags & kFileFlagO_ACCMODE) == kFileFlagO_RDWR)) {
        return -1;
    }
    if ((flags & kFileFlagO_TRUNC) != 0U && !file->read_only) {
        file->size = 0U;
    }

    for (size_t index = 0U; index < kMaxOpenFiles; ++index) {
        if (!g_open_files[index].in_use) {
            g_open_files[index].in_use = true;
            g_open_files[index].file = file;
            g_open_files[index].access = flags & kFileFlagO_ACCMODE;
            g_open_files[index].append_mode = (flags & kFileFlagO_APPEND) != 0U;
            g_open_files[index].offset = g_open_files[index].append_mode ? file->size : 0U;
            return static_cast<int>(kFirstFileDescriptor + index);
        }
    }
    return -1;
}

int read(int fd, void* buffer, uint32_t count) noexcept {
    if (buffer == nullptr || count == 0U) {
        return 0;
    }
    if (fd < kFirstFileDescriptor) {
        return -1;
    }

    const size_t slot = static_cast<size_t>(fd - kFirstFileDescriptor);
    if (slot >= kMaxOpenFiles || !g_open_files[slot].in_use || g_open_files[slot].file == nullptr) {
        return -1;
    }

    const OpenFile& file = g_open_files[slot];
    if (file.file->is_directory) {
        char listing[1024U]{};
        const uint32_t listing_size = build_directory_listing(
            file.file->path,
            listing,
            static_cast<uint32_t>(sizeof(listing)));
        if (file.offset >= listing_size) {
            return 0;
        }
        uint32_t available = listing_size - file.offset;
        if (count > available) {
            count = available;
        }
        auto* output = static_cast<uint8_t*>(buffer);
        for (uint32_t index = 0U; index < count; ++index) {
            output[index] = static_cast<uint8_t>(listing[file.offset + index]);
        }
        g_open_files[slot].offset += count;
        return static_cast<int>(count);
    }
    if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_WRONLY) {
        return -1;
    }
    if (file.offset >= file.file->size) {
        return 0;
    }

    uint32_t available = file.file->size - file.offset;
    if (count > available) {
        count = available;
    }
    auto* output = static_cast<uint8_t*>(buffer);
    for (uint32_t index = 0U; index < count; ++index) {
        output[index] = file.file->data[file.offset + index];
    }
    g_open_files[slot].offset += count;
    return static_cast<int>(count);
}

int write(int fd, const void* buffer, uint32_t count) noexcept {
    if (buffer == nullptr || count == 0U) {
        return 0;
    }
    if (fd < kFirstFileDescriptor) {
        return -1;
    }

    const size_t slot = static_cast<size_t>(fd - kFirstFileDescriptor);
    if (slot >= kMaxOpenFiles || !g_open_files[slot].in_use || g_open_files[slot].file == nullptr) {
        return -1;
    }

    OpenFile& file = g_open_files[slot];
    if (file.file->is_directory) {
        return -1;
    }
    if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_RDONLY) {
        return -1;
    }
    if (file.file->read_only) {
        return -1;
    }
    if (file.append_mode) {
        file.offset = file.file->size;
    }

    const auto* input = static_cast<const uint8_t*>(buffer);
    if (file.offset + count > file.file->capacity) {
        count = file.file->capacity - file.offset;
    }
    if (count == 0U) {
        return 0;
    }
    for (uint32_t index = 0U; index < count; ++index) {
        file.file->data[file.offset + index] = input[index];
    }
    file.offset += count;
    if (file.offset > file.file->size) {
        file.file->size = file.offset;
    }
    return static_cast<int>(count);
}

int close(int fd) noexcept {
    if (fd < kFirstFileDescriptor) {
        return -1;
    }

    const size_t slot = static_cast<size_t>(fd - kFirstFileDescriptor);
    if (slot >= kMaxOpenFiles || !g_open_files[slot].in_use) {
        return -1;
    }

    g_open_files[slot] = {false, nullptr, 0U, kFileFlagO_RDONLY, false};
    return 0;
}

int access(const char* path) noexcept {
    char normalized[kMaxPathLength]{};
    if (!normalize_path(path, normalized, static_cast<uint32_t>(sizeof(normalized)))) {
        return -1;
    }
    return find(normalized) != nullptr ? 0 : -1;
}

} // namespace xinim::kernel::bootfs

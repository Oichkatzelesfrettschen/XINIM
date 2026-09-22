#include "bootfs.hpp"

#include "console.hpp"
#include "devfs.hpp"
#if !defined(XINIM_ARCH_X86_64)
#include "procfs.hpp"
#endif
#include "ext2_reader.hpp"
#include "tty.hpp"
#include "vfs.hpp"

#include <cerrno>

extern "C" void *memset(void *destination, int value, unsigned long count);

namespace xinim::kernel::bootfs {
    namespace ext2_reader = ::xinim::i486::ext2_reader;
    namespace {

        constexpr size_t kMaxFiles = kMaximumFileRecords;
        constexpr size_t kMaxDynamicFiles = kMaximumDynamicNodes;
        constexpr size_t kMaxSymbolicLinks = kMaximumSymbolicLinks;
        constexpr size_t kMaxUserOpenFiles = kMaximumBackendUserOpenFiles;
        constexpr size_t kReservedOpenDescriptors = 3U;
        constexpr size_t kMaxOpenFiles = kReservedOpenDescriptors + kMaxUserOpenFiles;
        constexpr size_t kMaxPathLength = 256U;
        constexpr size_t kPathCacheEntries = 16U;
        constexpr size_t kTmpPathLength = 256U;
        constexpr size_t kTmpFileCapacity = 65536U;
        constexpr size_t kMaxMounts = 4U;
        constexpr size_t kMaxPipes = 16U;
        constexpr size_t kPipeCapacity = kPipeBufferSize;
        constexpr uint32_t kMaxPipeIdCounter = 1024U;
        constexpr uint32_t kMaximumSymbolicLinkDepth = 40U;
        constexpr int kFirstFileDescriptor = static_cast<int>(kReservedOpenDescriptors);

        constexpr uint32_t kFileFlagO_RDONLY = 0x0000U;
        constexpr uint32_t kFileFlagO_WRONLY = 0x0001U;
        constexpr uint32_t kFileFlagO_RDWR = 0x0002U;
        constexpr uint32_t kFileFlagO_APPEND = 0x0400U;
        constexpr uint32_t kFileFlagO_CREAT = 0x0040U;
        constexpr uint32_t kFileFlagO_TRUNC = 0x0200U;
        constexpr uint32_t kFileFlagO_EXCL = 0x0080U;
        constexpr uint32_t kFileFlagO_ACCMODE = 0x0003U;
        constexpr uint32_t kFileFlagO_NONBLOCK = 0x0800U;

        constexpr int kFdCloExec = 1;

        constexpr int kSeekSet = 0;
        constexpr int kSeekCur = 1;
        constexpr int kSeekEnd = 2;

        constexpr unsigned long kTermiosGet = 0x5401UL;
        constexpr unsigned long kTermiosSetNow = 0x5402UL;
        constexpr unsigned long kTermiosSetDrain = 0x5403UL;
        constexpr unsigned long kTermiosSetFlush = 0x5404UL;
        constexpr unsigned long kTtyGetPgrp = 0x540FUL;
        constexpr unsigned long kTtySetPgrp = 0x5410UL;
        constexpr unsigned long kWindowSizeGet = 0x5413UL;
        constexpr unsigned long kWindowSizeSet = 0x5414UL;

        struct TermiosState {
            uint32_t c_iflag;
            uint32_t c_oflag;
            uint32_t c_cflag;
            uint32_t c_lflag;
            uint8_t c_line;
            uint8_t c_cc[19];
        };

        struct WindowSize {
            uint16_t ws_row;
            uint16_t ws_col;
            uint16_t ws_xpixel;
            uint16_t ws_ypixel;
        };

        enum class DeviceType : uint8_t {
            None = 0,
            DevNull = 1,
            DevZero = 2,
            VfsDelegate = 3,
        };

        struct OpenFile {
            bool in_use;
            FileRecord *file;
            uint32_t offset;
            uint32_t access;
            uint32_t status_flags;
            int descriptor_flags;
            bool append_mode;
            bool is_pipe;
            bool is_console;
            uint32_t pipe_id;
            bool is_pipe_writer;
            bool is_ext2;
            bool ext2_is_directory;
            uint32_t ext2_size;
            uint32_t refcount; // Number of fd_map entries referencing this slot
            DeviceType device_type;
            xinim::i486::vfs::VfsOps *vfs_ops;
            int vfs_slot;
            char ext2_path[kMaxPathLength];
            bool is_bootfs_directory;
            char dir_path[kMaxPathLength]; // Path of opened directory for getdents
        };

        struct Pipe {
            bool in_use;
            uint32_t id;
            uint32_t readers;
            uint32_t writers;
            uint32_t start;
            uint32_t count;
            uint8_t data[kPipeCapacity];
        };

        struct RamFile {
            bool in_use;
            char path[kTmpPathLength];
            uint8_t data[kTmpFileCapacity];
            uint32_t size;
            bool executable;
            bool is_directory;
        };

        struct SymbolicLinkRecord {
            bool in_use;
            char path[kTmpPathLength];
            uint8_t target[kTmpPathLength];
        };

        struct PathCacheEntry {
            bool valid;
            bool negative;
            char path[kMaxPathLength];
            const FileRecord *file;
        };

        struct MountPoint {
            bool in_use;
            char path[kMaxPathLength];
        };

        FileRecord g_files[kMaxFiles]{};
        OpenFile g_open_files[kMaxOpenFiles]{};
        RamFile g_tmp_files[kMaxDynamicFiles]{};
        SymbolicLinkRecord g_symbolic_links[kMaxSymbolicLinks]{};
        PathCacheEntry g_path_cache[kPathCacheEntries]{};
        Pipe g_pipes[kMaxPipes]{};
        MountPoint g_mount_points[kMaxMounts]{};
        uint32_t g_file_count = 0U;
        uint32_t g_next_pipe_id = 0U;
        bool g_pipe_eof_event = false;
        TermiosState g_terminal_state{};
        WindowSize g_window_size{25U, 80U, 0U, 0U};
        int g_foreground_pgrp = 1;

        template <typename T> void zero_object(T &object) noexcept {
            static_cast<void>(::memset(&object, 0, sizeof(T)));
        }

        OpenFile make_empty_open_file() noexcept {
            return {
                false,
                nullptr,
                0U,
                kFileFlagO_RDONLY,
                kFileFlagO_RDONLY,
                0,
                false,
                false,
                false,
                0U,
                false,
                false,
                false,
                0U,
                0U,
                DeviceType::None,
                nullptr,
                0,
                {},
                false,
                {},
            };
        }

        void configure_open_file_entry(size_t slot, FileRecord *file, uint32_t access, bool append,
                                       bool is_pipe, bool is_console, uint32_t pipe_id,
                                       bool is_pipe_writer) noexcept;
        [[nodiscard]] bool parent_directory_exists(const char *path) noexcept;
        void release_unlinked_file_record(FileRecord *file) noexcept;

        void initialize_terminal_state() noexcept {
            g_terminal_state = {};
            g_terminal_state.c_iflag = 0002400U;
            g_terminal_state.c_oflag = 0000005U;
            g_terminal_state.c_cflag = 0000277U;
            g_terminal_state.c_lflag = 0001053U;
            g_terminal_state.c_cc[0] = 3U;
            g_terminal_state.c_cc[1] = 28U;
            g_terminal_state.c_cc[2] = 127U;
            g_terminal_state.c_cc[3] = 21U;
            g_terminal_state.c_cc[4] = 4U;
            g_terminal_state.c_cc[5] = 0U;
            g_terminal_state.c_cc[6] = 1U;
            g_terminal_state.c_cc[8] = 17U;
            g_terminal_state.c_cc[9] = 19U;
            g_terminal_state.c_cc[10] = 26U;
        }

        Pipe *get_pipe(uint32_t pipe_id) noexcept {
            const uint32_t index = pipe_id % kMaxPipes;
            if (!g_pipes[index].in_use || g_pipes[index].id == kMaxPipeIdCounter) {
                return nullptr;
            }
            if (g_pipes[index].id != pipe_id) {
                for (uint32_t candidate = 0U; candidate < kMaxPipes; ++candidate) {
                    if (g_pipes[candidate].in_use && g_pipes[candidate].id == pipe_id) {
                        return &g_pipes[candidate];
                    }
                }
                return nullptr;
            }
            return &g_pipes[index];
        }

        bool is_open_file_valid(size_t slot) noexcept {
            return slot < kMaxOpenFiles && g_open_files[slot].in_use &&
                   (g_open_files[slot].file != nullptr || g_open_files[slot].is_pipe ||
                    g_open_files[slot].is_console || g_open_files[slot].is_ext2 ||
                    g_open_files[slot].device_type != DeviceType::None);
        }

        bool is_pipe_slot_busy(size_t slot) noexcept {
            if (!is_open_file_valid(slot)) {
                return false;
            }
            return g_open_files[slot].is_pipe;
        }

        [[nodiscard]] bool is_fd_valid(int fd) noexcept {
            return fd >= 0 && static_cast<size_t>(fd) < kMaxOpenFiles;
        }

        size_t fd_to_slot(int fd) noexcept {
            return static_cast<size_t>(fd);
        }

        uint32_t write_pipe_buffer(Pipe &pipe, const uint8_t *data, uint32_t count) noexcept {
            uint32_t written = 0U;
            while (written < count && pipe.count < kPipeCapacity) {
                pipe.data[(pipe.start + pipe.count) % kPipeCapacity] = data[written];
                ++pipe.count;
                ++written;
            }
            return written;
        }

        uint32_t read_pipe_buffer(Pipe &pipe, uint8_t *out, uint32_t count) noexcept {
            uint32_t read = 0U;
            if (pipe.count == 0U) {
                return 0U;
            }
            while (read < count && pipe.count > 0U) {
                out[read] = pipe.data[pipe.start];
                ++read;
                --pipe.count;
                pipe.start = (pipe.start + 1U) % kPipeCapacity;
            }
            return read;
        }

        void close_open_file_slot(size_t slot) noexcept {
            if (!is_open_file_valid(slot)) {
                return;
            }

            if (is_pipe_slot_busy(slot)) {
                Pipe *pipe = get_pipe(g_open_files[slot].pipe_id);
                if (pipe != nullptr) {
                    if (g_open_files[slot].is_pipe_writer) {
                        if (pipe->writers > 0U) {
                            --pipe->writers;
                            if (pipe->writers == 0U) {
                                g_pipe_eof_event = true;
                            }
                        }
                    } else {
                        if (pipe->readers > 0U) {
                            --pipe->readers;
                        }
                    }
                    if (pipe->writers == 0U && pipe->readers == 0U) {
                        pipe->in_use = false;
                    }
                }
            }

            // Close VFS delegate slot if applicable
            if (g_open_files[slot].device_type == DeviceType::VfsDelegate &&
                g_open_files[slot].vfs_ops != nullptr &&
                g_open_files[slot].vfs_ops->close != nullptr) {
                g_open_files[slot].vfs_ops->close(g_open_files[slot].vfs_slot);
            }

            FileRecord *closed_file = g_open_files[slot].file;
            g_open_files[slot] = make_empty_open_file();
            release_unlinked_file_record(closed_file);
        }

        [[nodiscard]] bool string_equals(const char *lhs, const char *rhs) noexcept {
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

        [[nodiscard]] uint32_t string_length(const char *text) noexcept {
            uint32_t length = 0U;
            if (text == nullptr) {
                return 0U;
            }
            while (text[length] != '\0') {
                ++length;
            }
            return length;
        }

        void copy_c_string(char *destination, uint32_t capacity, const char *source) noexcept {
            if (destination == nullptr || capacity == 0U)
                return;
            if (source == nullptr) {
                destination[0] = '\0';
                return;
            }
            uint32_t index = 0U;
            while (index + 1U < capacity && source[index] != '\0') {
                destination[index] = source[index];
                ++index;
            }
            destination[index] = '\0';
        }

        uint32_t hash_path(const char *path) noexcept {
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

        bool mount_exists(const char *path) noexcept {
            if (path == nullptr) {
                return false;
            }
            for (size_t index = 0U; index < kMaxMounts; ++index) {
                if (g_mount_points[index].in_use &&
                    string_equals(g_mount_points[index].path, path)) {
                    return true;
                }
            }
            return false;
        }

        PathCacheEntry *path_cache_slot(const char *path) noexcept {
            return &g_path_cache[hash_path(path) % kPathCacheEntries];
        }

        const FileRecord *cache_lookup(const char *path, bool &hit) noexcept {
            hit = false;
            if (path == nullptr) {
                return nullptr;
            }
            PathCacheEntry *entry = path_cache_slot(path);
            if (!entry->valid || !string_equals(entry->path, path)) {
                return nullptr;
            }
            hit = true;
            return entry->negative ? nullptr : entry->file;
        }

        void cache_store(const char *path, const FileRecord *file) noexcept {
            if (path == nullptr) {
                return;
            }
            PathCacheEntry *entry = path_cache_slot(path);
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

        bool normalize_path(const char *input, char *output, uint32_t capacity) noexcept {
            if (input == nullptr || output == nullptr || capacity < 2U || input[0] == '\0') {
                return false;
            }

            output[0] = '/';
            uint32_t output_length = 1U;
            uint32_t cursor = input[0] == '/' ? 1U : 0U;
            while (input[cursor] != '\0') {
                while (input[cursor] == '/') {
                    ++cursor;
                }
                if (input[cursor] == '\0') {
                    break;
                }

                const uint32_t component_start = cursor;
                while (input[cursor] != '\0' && input[cursor] != '/') {
                    ++cursor;
                }
                const uint32_t component_length = cursor - component_start;
                const bool is_current_directory =
                    component_length == 1U && input[component_start] == '.';
                const bool is_parent_directory = component_length == 2U &&
                                                 input[component_start] == '.' &&
                                                 input[component_start + 1U] == '.';
                if (is_current_directory) {
                    continue;
                }
                if (is_parent_directory) {
                    while (output_length > 1U && output[output_length - 1U] != '/') {
                        --output_length;
                    }
                    if (output_length > 1U) {
                        --output_length;
                    }
                    continue;
                }

                const uint32_t separator_length = output_length > 1U ? 1U : 0U;
                if (output_length + separator_length + component_length >= capacity) {
                    return false;
                }
                if (separator_length != 0U) {
                    output[output_length++] = '/';
                }
                for (uint32_t component_index = 0U; component_index < component_length;
                     ++component_index) {
                    output[output_length++] = input[component_start + component_index];
                }
            }
            output[output_length] = '\0';

            return true;
        }

        bool starts_with(const char *text, const char *prefix) noexcept {
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

        [[nodiscard]] bool is_tmp_path(const char *path) noexcept {
            if (path == nullptr) {
                return false;
            }
            if (string_equals(path, "/tmp")) {
                return true;
            }
            return starts_with(path, "/tmp/");
        }

        [[nodiscard]] uint32_t child_name_length(const char *path, const char *child) noexcept {
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
                const char *cursor = child + 1U;
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
            const char *base = child + dir_len + 1U;
            if (*base == '\0') {
                return 0U;
            }
            const char *rest = base;
            while (*rest != '\0') {
                if (*rest == '/') {
                    return 0U;
                }
                ++rest;
            }
            return string_length(base);
        }

        [[nodiscard]] bool copy_child_name(const char *path, const char *child, char *output,
                                           uint32_t index, uint32_t capacity,
                                           uint32_t &next_index) noexcept {
            const uint32_t name_length = child_name_length(path, child);
            if (name_length == 0U || output == nullptr) {
                return false;
            }
            const char *base = child;
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

        [[nodiscard]] bool write_directory_entry(char *destination, uint32_t capacity,
                                                 uint32_t &index, const char *entry_name,
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

        [[nodiscard]] uint32_t build_directory_listing(const char *directory_path, char *buffer,
                                                       uint32_t capacity) noexcept {
            if (directory_path == nullptr || buffer == nullptr || capacity == 0U) {
                return 0U;
            }
            uint32_t index = 0U;
            buffer[0] = '\0';

            const char *path = directory_path;
            if (!is_directory(path)) {
                return 0U;
            }

            for (uint32_t entry = 0U; entry < g_file_count; ++entry) {
                const char *candidate = g_files[entry].path;
                if (candidate == nullptr) {
                    continue;
                }
                char child_name[kTmpPathLength]{};
                uint32_t child_index = 0U;
                if (!copy_child_name(path, candidate, child_name, 0U, sizeof(child_name),
                                     child_index)) {
                    continue;
                }
                bool is_directory_entry = g_files[entry].is_directory;
                if (!write_directory_entry(buffer, capacity, index, child_name,
                                           is_directory_entry)) {
                    break;
                }
            }
            for (uint32_t entry = 0U; entry < kMaxMounts; ++entry) {
                if (!g_mount_points[entry].in_use) {
                    continue;
                }
                char child_name[kTmpPathLength]{};
                uint32_t child_index = 0U;
                if (!copy_child_name(path, g_mount_points[entry].path, child_name, 0U,
                                     sizeof(child_name), child_index)) {
                    continue;
                }
                if (!write_directory_entry(buffer, capacity, index, child_name, true)) {
                    break;
                }
            }
            return index;
        }

        void configure_ext2_open_file(size_t slot, const char *path, bool is_directory,
                                      uint32_t size, uint32_t access) noexcept {
            configure_open_file_entry(slot, nullptr, access, false, false, false, 0U, false);
            auto &open_file = g_open_files[slot];
            open_file.is_ext2 = true;
            open_file.ext2_is_directory = is_directory;
            open_file.ext2_size = size;
            const uint32_t length = string_length(path);
            for (uint32_t index = 0U; index < sizeof(open_file.ext2_path); ++index) {
                open_file.ext2_path[index] = index < length ? path[index] : '\0';
                if (index >= length) {
                    break;
                }
            }
            if (is_directory) {
                copy_c_string(open_file.dir_path, static_cast<uint32_t>(sizeof(open_file.dir_path)),
                              path);
            }
        }

        [[nodiscard]] bool file_record_is_open(const FileRecord *file) noexcept {
            if (file == nullptr) {
                return false;
            }
            for (size_t slot = 0U; slot < kMaxOpenFiles; ++slot) {
                if (g_open_files[slot].in_use && g_open_files[slot].file == file) {
                    return true;
                }
            }
            return false;
        }

        FileRecord *allocate_file_record() noexcept {
            for (uint32_t index = 0U; index < g_file_count; ++index) {
                if (g_files[index].path == nullptr && g_files[index].data == nullptr &&
                    !file_record_is_open(&g_files[index])) {
                    zero_object(g_files[index]);
                    return &g_files[index];
                }
            }
            if (g_file_count >= kMaxFiles) {
                return nullptr;
            }
            FileRecord *file = &g_files[g_file_count++];
            zero_object(*file);
            return file;
        }

        void release_file_storage(FileRecord &file) noexcept {
            for (size_t slot = 0U; slot < kMaxDynamicFiles; ++slot) {
                if (g_tmp_files[slot].in_use && g_tmp_files[slot].data == file.data) {
                    zero_object(g_tmp_files[slot]);
                    zero_object(file);
                    return;
                }
            }
            for (size_t slot = 0U; slot < kMaxSymbolicLinks; ++slot) {
                if (g_symbolic_links[slot].in_use && g_symbolic_links[slot].target == file.data) {
                    zero_object(g_symbolic_links[slot]);
                    zero_object(file);
                    return;
                }
            }
        }

        void release_unlinked_file_record(FileRecord *file) noexcept {
            if (file == nullptr || file->path != nullptr || file->data == nullptr) {
                return;
            }
            if (file_record_is_open(file)) {
                return;
            }
            release_file_storage(*file);
        }

        void reset() noexcept {
            g_file_count = 0U;
            g_next_pipe_id = 1U;
            reset_path_cache();
            initialize_terminal_state();
            xinim::i486::tty::initialize();
            xinim::i486::vfs::initialize();
            xinim::i486::vfs::mount("/dev", xinim::i486::devfs::ops());
#if !defined(XINIM_ARCH_X86_64)
            xinim::i486::vfs::mount("/proc", xinim::i486::procfs::ops());
#endif
            g_window_size = {25U, 80U, 0U, 0U};
            g_foreground_pgrp = 1;
            for (size_t index = 0U; index < kMaxFiles; ++index) {
                g_files[index].path = nullptr;
                g_files[index].data = nullptr;
                g_files[index].size = 0U;
                g_files[index].capacity = 0U;
                g_files[index].read_only = false;
                g_files[index].executable = false;
                g_files[index].is_directory = false;
                g_files[index].is_symlink = false;
                g_files[index].user_id = 0U;
                g_files[index].group_id = 0U;
                g_files[index].exclusive_lock_owner = 0U;
                g_files[index].shared_lock_count = 0U;
            }
            for (size_t index = 0U; index < kMaxOpenFiles; ++index) {
                zero_object(g_open_files[index]);
                g_open_files[index].access = kFileFlagO_RDONLY;
                g_open_files[index].status_flags = kFileFlagO_RDONLY;
                g_open_files[index].descriptor_flags = 0;
                g_open_files[index].device_type = DeviceType::None;
            }
            for (size_t index = 0U; index < kMaxDynamicFiles; ++index) {
                zero_object(g_tmp_files[index]);
            }
            for (size_t index = 0U; index < kMaxSymbolicLinks; ++index) {
                zero_object(g_symbolic_links[index]);
            }
            for (size_t index = 0U; index < kMaxPipes; ++index) {
                zero_object(g_pipes[index]);
                g_pipes[index].id = kMaxPipeIdCounter;
            }
            for (size_t index = 0U; index < kMaxMounts; ++index) {
                zero_object(g_mount_points[index]);
            }
            configure_open_file_entry(0U, nullptr, kFileFlagO_RDONLY, false, false, true, 0U,
                                      false);
            configure_open_file_entry(1U, nullptr, kFileFlagO_WRONLY, false, false, true, 0U,
                                      false);
            configure_open_file_entry(2U, nullptr, kFileFlagO_WRONLY, false, false, true, 0U,
                                      false);
        }

        void add_directory(const char *path) noexcept {
            if (path == nullptr) {
                return;
            }

            reset_path_cache();

            FileRecord *file = allocate_file_record();
            if (file == nullptr) {
                return;
            }
            file->path = path;
            file->read_only = true;
            file->is_directory = true;
        }

        FileRecord *add_file(const char *path, uint8_t *data, uint32_t size,
                             bool executable) noexcept {
            if (path == nullptr || data == nullptr || size == 0U) {
                return nullptr;
            }

            reset_path_cache();

            FileRecord *file = allocate_file_record();
            if (file == nullptr) {
                return nullptr;
            }
            file->path = path;
            file->data = data;
            file->size = size;
            file->capacity = size;
            file->read_only = true;
            file->executable = executable;
            return file;
        }

        FileRecord *create_tmp_file(const char *path) noexcept {
            if (!is_tmp_path(path)) {
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
                g_tmp_files[slot].is_directory = false;

                FileRecord *file = allocate_file_record();
                if (file == nullptr) {
                    zero_object(g_tmp_files[slot]);
                    return nullptr;
                }
                reset_path_cache();
                file->path = g_tmp_files[slot].path;
                file->data = g_tmp_files[slot].data;
                file->capacity = kTmpFileCapacity;
                return file;
            }
            return nullptr;
        }

        FileRecord *get_tmp_file(const char *path) noexcept {
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

        void configure_open_file_entry(size_t slot, FileRecord *file, uint32_t access, bool append,
                                       bool is_pipe, bool is_console, uint32_t pipe_id,
                                       bool is_pipe_writer) noexcept;

        int allocate_open_slot() noexcept {
            for (size_t index = static_cast<size_t>(kFirstFileDescriptor); index < kMaxOpenFiles;
                 ++index) {
                if (!g_open_files[index].in_use) {
                    g_open_files[index] = make_empty_open_file();
                    g_open_files[index].in_use = true;
                    g_open_files[index].refcount = 1U;
                    return static_cast<int>(index);
                }
            }
            return -EMFILE;
        }

        void configure_open_file_entry(size_t slot, FileRecord *file, uint32_t access, bool append,
                                       bool is_pipe, bool is_console, uint32_t pipe_id,
                                       bool is_pipe_writer) noexcept {
            auto &open_file = g_open_files[slot];
            open_file.in_use = true;
            open_file.file = file;
            open_file.access = access;
            open_file.status_flags = access;
            open_file.descriptor_flags = 0;
            open_file.is_pipe = is_pipe;
            open_file.is_console = is_console;
            open_file.pipe_id = pipe_id;
            open_file.is_pipe_writer = is_pipe_writer;
            open_file.is_ext2 = false;
            open_file.ext2_is_directory = false;
            open_file.ext2_size = 0U;
            open_file.refcount = 1U;
            open_file.device_type = DeviceType::None;
            __builtin_memset(open_file.ext2_path, 0, sizeof(open_file.ext2_path));
            open_file.is_bootfs_directory = false;
            __builtin_memset(open_file.dir_path, 0, sizeof(open_file.dir_path));
            open_file.offset = append ? ((file != nullptr) ? file->size : 0U) : 0U;
            open_file.append_mode = (!is_pipe) && append;
            if (!is_pipe && append) {
                open_file.status_flags |= kFileFlagO_APPEND;
            }
        }

        Pipe *allocate_pipe() noexcept {
            if (kMaxPipes == 0U) {
                return nullptr;
            }

            for (size_t attempts = 0U; attempts < kMaxPipes * 2U; ++attempts) {
                const uint32_t candidate_id = g_next_pipe_id;
                const uint32_t candidate_index = candidate_id % static_cast<uint32_t>(kMaxPipes);
                if (candidate_id == 0U || candidate_id > kMaxPipeIdCounter) {
                    g_next_pipe_id = 1U;
                }

                auto &pipe = g_pipes[candidate_index];
                if (!pipe.in_use || (candidate_id != pipe.id)) {
                    pipe.in_use = true;
                    pipe.id = candidate_id;
                    pipe.readers = 0U;
                    pipe.writers = 0U;
                    pipe.start = 0U;
                    pipe.count = 0U;
                    ++g_next_pipe_id;
                    return &pipe;
                }

                ++g_next_pipe_id;
            }
            return nullptr;
        }

    } // namespace

    void initialize(const xinim::boot::BootInfo &info) noexcept {
        reset();

        add_directory("/");
        add_directory("/bin");
        add_directory("/boot");
        add_directory("/dev");
        add_directory("/etc");
        add_directory("/tmp");

        for (size_t index = 0U; index < info.modules_count; ++index) {
            const xinim::boot::BootModule &module = info.modules[index];
            if (!module.valid()) {
                continue;
            }

            auto *data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(module.address));
            const uint32_t size = static_cast<uint32_t>(module.size);
            FileRecord *file =
                add_file(module.string, data, size, starts_with(module.string, "/bin/"));
#if !defined(XINIM_ARCH_X86_64)
            if (file != nullptr && string_equals(module.string, "/bin/xash")) {
                FileRecord *sh_file = add_file("/bin/sh", data, size, true);
                (void) sh_file;
            } else if (file != nullptr && string_equals(module.string, "/boot/xash")) {
                FileRecord *xash_file = add_file("/bin/xash", data, size, true);
                FileRecord *sh_file = add_file("/bin/sh", data, size, true);
                FileRecord *mksh_file = add_file("/bin/mksh", data, size, true);
                (void) xash_file;
                (void) sh_file;
                (void) mksh_file;
            } else if (file != nullptr && string_equals(module.string, "/bin/mksh")) {
                FileRecord *mksh_file = add_file("/boot/mksh", data, size, true);
                (void) mksh_file;
            }
#else
            (void) file;
#endif
        }
    }

    const FileRecord *find_normalized_record(const char *normalized) noexcept {
        bool cache_hit = false;
        const FileRecord *cached = cache_lookup(normalized, cache_hit);
        if (cache_hit) {
            return cached;
        }

        for (size_t index = 0U; index < g_file_count; ++index) {
            if (string_equals(g_files[index].path, normalized)) {
                cache_store(normalized, &g_files[index]);
                return &g_files[index];
            }
        }
        const FileRecord *file = get_tmp_file(normalized);
        cache_store(normalized, file);
        return file;
    }

    bool replace_symbolic_link_component(const char *path, uint32_t component_start,
                                         uint32_t component_end, const FileRecord &symbolic_link,
                                         char *output, uint32_t capacity) noexcept {
        if (symbolic_link.data == nullptr || symbolic_link.size == 0U ||
            symbolic_link.size >= capacity) {
            return false;
        }

        uint32_t output_length = 0U;
        auto append_character = [&](char character) noexcept {
            if (output_length + 1U >= capacity) {
                return false;
            }
            output[output_length++] = character;
            return true;
        };
        const char *target = reinterpret_cast<const char *>(symbolic_link.data);
        if (target[0] != '/') {
            const uint32_t parent_length = component_start > 1U ? component_start - 1U : 1U;
            for (uint32_t index = 0U; index < parent_length; ++index) {
                if (!append_character(path[index])) {
                    return false;
                }
            }
            if (output_length == 0U || output[output_length - 1U] != '/') {
                if (!append_character('/')) {
                    return false;
                }
            }
        }
        for (uint32_t index = 0U; index < symbolic_link.size; ++index) {
            if (!append_character(target[index])) {
                return false;
            }
        }
        for (uint32_t index = component_end; path[index] != '\0'; ++index) {
            if (!append_character(path[index])) {
                return false;
            }
        }
        output[output_length] = '\0';
        return true;
    }

    bool resolve_bootfs_symbolic_links(const char *path, char *resolved, uint32_t capacity,
                                       bool follow_final) noexcept {
        char current[kMaxPathLength]{};
        if (!normalize_path(path, current, static_cast<uint32_t>(sizeof(current)))) {
            return false;
        }

        for (uint32_t depth = 0U; depth < kMaximumSymbolicLinkDepth; ++depth) {
            bool replaced = false;
            uint32_t component_start = 1U;
            for (uint32_t cursor = 1U;; ++cursor) {
                const bool at_end = current[cursor] == '\0';
                if (!at_end && current[cursor] != '/') {
                    continue;
                }
                char prefix[kMaxPathLength]{};
                for (uint32_t index = 0U; index < cursor; ++index) {
                    prefix[index] = current[index];
                }
                prefix[cursor] = '\0';
                const FileRecord *record = find_normalized_record(prefix);
                if (record != nullptr && record->is_symlink && (follow_final || !at_end)) {
                    char replacement[kMaxPathLength]{};
                    char normalized_replacement[kMaxPathLength]{};
                    if (!replace_symbolic_link_component(
                            current, component_start, cursor, *record, replacement,
                            static_cast<uint32_t>(sizeof(replacement))) ||
                        !normalize_path(replacement, normalized_replacement,
                                        static_cast<uint32_t>(sizeof(normalized_replacement)))) {
                        return false;
                    }
                    copy_c_string(current, static_cast<uint32_t>(sizeof(current)),
                                  normalized_replacement);
                    replaced = true;
                    break;
                }
                if (at_end) {
                    copy_c_string(resolved, capacity, current);
                    return true;
                }
                component_start = cursor + 1U;
            }
            if (!replaced) {
                copy_c_string(resolved, capacity, current);
                return true;
            }
        }
        return false;
    }

    const FileRecord *find(const char *path) noexcept {
        char resolved[kMaxPathLength]{};
        return resolve_bootfs_symbolic_links(path, resolved,
                                             static_cast<uint32_t>(sizeof(resolved)), true)
                   ? find_normalized_record(resolved)
                   : nullptr;
    }

    void for_each_entry(VisitCallback callback, void *context) noexcept {
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

    int register_directory(const char *path) noexcept {
        if (path == nullptr) {
            return -1;
        }
        if (find(path) != nullptr) {
            return 0;
        }
        add_directory(path);
        return find(path) != nullptr ? 0 : -1;
    }

    int register_mount_directory(const char *path) noexcept {
        char normalized[kMaxPathLength]{};
        if (!normalize_path(path, normalized, static_cast<uint32_t>(sizeof(normalized)))) {
            return -1;
        }
        if (mount_exists(normalized) || find(normalized) != nullptr) {
            return 0;
        }
        for (size_t index = 0U; index < kMaxMounts; ++index) {
            if (g_mount_points[index].in_use) {
                continue;
            }
            g_mount_points[index].in_use = true;
            for (size_t cursor = 0U; cursor < kMaxPathLength; ++cursor) {
                g_mount_points[index].path[cursor] = normalized[cursor];
                if (normalized[cursor] == '\0') {
                    break;
                }
            }
            return 0;
        }
        return -1;
    }

    int register_read_only_file(const char *path, uint8_t *data, uint32_t size,
                                bool executable) noexcept {
        if (path == nullptr || data == nullptr || size == 0U) {
            return -1;
        }
        if (find(path) != nullptr) {
            return 0;
        }
        return add_file(path, data, size, executable) != nullptr ? 0 : -1;
    }

    bool is_directory(const char *path) noexcept {
        char resolved[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, resolved, static_cast<uint32_t>(sizeof(resolved)),
                                           true)) {
            return false;
        }
        ext2_reader::NodeInfo ext2_info{};
        if (ext2_reader::query_runtime_path(resolved, ext2_info)) {
            return ext2_info.is_directory;
        }
        const FileRecord *file = find_normalized_record(resolved);
        return file != nullptr && file->is_directory;
    }

    void close_cloexec_fds() noexcept {
        for (size_t index = kReservedOpenDescriptors; index < kMaxOpenFiles; ++index) {
            if (g_open_files[index].in_use &&
                (g_open_files[index].descriptor_flags & kFdCloExec) != 0) {
                g_open_files[index].in_use = false;
            }
        }
    }

    const char *directory_path_for_fd(int fd) noexcept {
        if (!is_fd_valid(fd))
            return nullptr;
        const size_t slot = fd_to_slot(fd);
        if (!g_open_files[slot].in_use)
            return nullptr;
        // Check ext2 directory
        if (g_open_files[slot].is_ext2 && g_open_files[slot].ext2_is_directory) {
            return g_open_files[slot].ext2_path;
        }
        // Check bootfs directory
        if (g_open_files[slot].is_bootfs_directory && g_open_files[slot].dir_path[0] != '\0') {
            return g_open_files[slot].dir_path;
        }
        // Legacy: if it's a bootfs FileRecord directory
        if (g_open_files[slot].file != nullptr && g_open_files[slot].file->is_directory) {
            return g_open_files[slot].file->path;
        }
        return nullptr;
    }

    int open(const char *path, uint32_t flags, uint32_t mode) noexcept {
        char normalized[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, normalized,
                                           static_cast<uint32_t>(sizeof(normalized)), true)) {
            return -ENOENT;
        }

        // Route through VFS mount table (devfs, procfs, etc.)
        {
            const char *relative = nullptr;
            xinim::i486::vfs::VfsOps *vops = xinim::i486::vfs::resolve(normalized, &relative);
            if (vops != nullptr && vops->open != nullptr) {
                // Check for specific device nodes first
                const bool is_tty = string_equals(normalized, "/dev/tty") ||
                                    string_equals(normalized, "/dev/console");
                const bool is_null = string_equals(normalized, "/dev/null");
                const bool is_zero = string_equals(normalized, "/dev/zero");
                if (is_tty || is_null || is_zero) {
                    const int fd = allocate_open_slot();
                    if (fd < 0) {
                        return fd;
                    }
                    configure_open_file_entry(fd_to_slot(fd), nullptr, flags & kFileFlagO_ACCMODE,
                                              false, false, is_tty, 0U, false);
                    if (is_null) {
                        g_open_files[fd_to_slot(fd)].device_type = DeviceType::DevNull;
                    } else if (is_zero) {
                        g_open_files[fd_to_slot(fd)].device_type = DeviceType::DevZero;
                    }
                    return fd;
                }
                // Generic VFS delegate (procfs, etc.)
                const int vfs_slot = vops->open(relative, flags, mode);
                if (vfs_slot >= 0) {
                    const int fd = allocate_open_slot();
                    if (fd < 0) {
                        if (vops->close != nullptr)
                            vops->close(vfs_slot);
                        return fd;
                    }
                    auto &f = g_open_files[fd_to_slot(fd)];
                    f = {};
                    f.in_use = true;
                    f.device_type = DeviceType::VfsDelegate;
                    f.vfs_ops = vops;
                    f.vfs_slot = vfs_slot;
                    copy_c_string(f.ext2_path, sizeof(f.ext2_path), normalized);
                    return fd;
                }
                // VFS open returned error; fall through to ext2/bootfs
            }
        }

        ext2_reader::NodeInfo ext2_info{};
        if (ext2_reader::query_runtime_path(normalized, ext2_info)) {
            const uint32_t access_mode = flags & kFileFlagO_ACCMODE;
            if (ext2_info.is_directory && access_mode != kFileFlagO_RDONLY) {
                return -EISDIR;
            }
            if (!ext2_info.is_directory && (flags & kFileFlagO_TRUNC) != 0U &&
                !ext2_reader::truncate_runtime_file(normalized, 0U)) {
                return -EROFS;
            }
            if (!ext2_info.is_directory && (flags & kFileFlagO_TRUNC) != 0U) {
                ext2_info.size = 0U;
            }
            const int fd = allocate_open_slot();
            if (fd < 0) {
                return fd;
            }
            configure_ext2_open_file(fd_to_slot(fd), normalized, ext2_info.is_directory,
                                     ext2_info.size, access_mode);
            g_open_files[fd_to_slot(fd)].status_flags = access_mode | (flags & kFileFlagO_APPEND);
            g_open_files[fd_to_slot(fd)].append_mode =
                (!ext2_info.is_directory) && ((flags & kFileFlagO_APPEND) != 0U);
            g_open_files[fd_to_slot(fd)].offset =
                g_open_files[fd_to_slot(fd)].append_mode ? ext2_info.size : 0U;
            return fd;
        }
        if ((flags & kFileFlagO_CREAT) != 0U) {
            const uint32_t access_mode = flags & kFileFlagO_ACCMODE;
            if (ext2_reader::create_runtime_file(normalized, static_cast<uint16_t>(mode)) &&
                ext2_reader::query_runtime_path(normalized, ext2_info) && !ext2_info.is_directory) {
                if ((flags & kFileFlagO_TRUNC) != 0U &&
                    !ext2_reader::truncate_runtime_file(normalized, 0U)) {
                    return -EROFS;
                }
                if ((flags & kFileFlagO_TRUNC) != 0U) {
                    ext2_info.size = 0U;
                }
                const int fd = allocate_open_slot();
                if (fd < 0) {
                    return fd;
                }
                configure_ext2_open_file(fd_to_slot(fd), normalized, false, ext2_info.size,
                                         access_mode);
                g_open_files[fd_to_slot(fd)].status_flags =
                    access_mode | (flags & kFileFlagO_APPEND);
                g_open_files[fd_to_slot(fd)].append_mode = (flags & kFileFlagO_APPEND) != 0U;
                g_open_files[fd_to_slot(fd)].offset =
                    g_open_files[fd_to_slot(fd)].append_mode ? ext2_info.size : 0U;
                return fd;
            }
        }

        FileRecord *file = const_cast<FileRecord *>(find(normalized));
        const bool existed = file != nullptr;
        if (file == nullptr) {
            if ((flags & kFileFlagO_CREAT) == 0U) {
                return -ENOENT;
            }
            if (!is_tmp_path(normalized)) {
                return -EROFS;
            }
            if (!parent_directory_exists(normalized)) {
                return -ENOENT;
            }
            file = create_tmp_file(normalized);
            if (file == nullptr) {
                return -ENOSPC;
            }
        }

        if ((flags & kFileFlagO_EXCL) != 0U && (flags & kFileFlagO_CREAT) != 0U && existed) {
            return -EEXIST;
        }

        if (file->is_directory) {
            if ((flags & kFileFlagO_ACCMODE) != kFileFlagO_RDONLY) {
                return -EISDIR;
            }
        } else if (file->read_only && ((flags & kFileFlagO_ACCMODE) == kFileFlagO_WRONLY ||
                                       (flags & kFileFlagO_ACCMODE) == kFileFlagO_RDWR)) {
            return -EROFS;
        }

        const int fd = allocate_open_slot();
        if (fd < 0) {
            return fd;
        }
        const size_t slot = fd_to_slot(fd);

        if (file->is_directory) {
            configure_open_file_entry(slot, file, kFileFlagO_RDONLY, false, false, false, 0U,
                                      false);
            g_open_files[slot].is_bootfs_directory = true;
            copy_c_string(g_open_files[slot].dir_path,
                          static_cast<uint32_t>(sizeof(g_open_files[slot].dir_path)), normalized);
            return fd;
        }
        if ((flags & kFileFlagO_TRUNC) != 0U && !file->read_only) {
            file->size = 0U;
        }
        configure_open_file_entry(slot, file, flags & kFileFlagO_ACCMODE,
                                  (flags & kFileFlagO_APPEND) != 0U, false, false, 0U, false);
        return fd;
    }

    int read(int fd, void *buffer, uint32_t count) noexcept {
        if (buffer == nullptr || count == 0U) {
            return 0;
        }
        if (!is_fd_valid(fd)) {
            return -1;
        }

        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }
        const OpenFile &file = g_open_files[slot];

        if (file.device_type == DeviceType::DevNull) {
            return 0; // EOF
        }

        if (file.device_type == DeviceType::DevZero) {
            auto *output = static_cast<uint8_t *>(buffer);
            for (uint32_t index = 0U; index < count; ++index) {
                output[index] = 0U;
            }
            return static_cast<int>(count);
        }

        if (file.device_type == DeviceType::VfsDelegate) {
            if (file.vfs_ops != nullptr && file.vfs_ops->read != nullptr) {
                return file.vfs_ops->read(file.vfs_slot, buffer, count);
            }
            return -1;
        }

        if (file.is_console) {
            if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_WRONLY) {
                return -1;
            }
            // Read through the TTY line discipline
            const int result = xinim::i486::tty::read(buffer, count);
            if (result == -1) {
                return kReadWouldBlock;
            }
            return result;
        }

        if (file.is_pipe) {
            Pipe *pipe = get_pipe(file.pipe_id);
            if (pipe == nullptr) {
                return -1;
            }
            if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_WRONLY) {
                return -1;
            }
            if (pipe->count == 0U && pipe->writers == 0U) {
                return 0; // EOF: no writers left
            }
            if (pipe->count == 0U) {
                if ((file.status_flags & kFileFlagO_NONBLOCK) != 0U) {
                    return -11; // -EAGAIN
                }
                return kReadWouldBlock; // Caller must block and retry
            }
            return static_cast<int>(read_pipe_buffer(*pipe, static_cast<uint8_t *>(buffer), count));
        }

        if (file.is_ext2) {
            if (file.ext2_is_directory) {
                char listing[1024U]{};
                const uint32_t listing_size = ext2_reader::build_runtime_directory_listing(
                    file.ext2_path, listing, static_cast<uint32_t>(sizeof(listing)));
                if (file.offset >= listing_size) {
                    return 0;
                }
                uint32_t available = listing_size - file.offset;
                if (count > available) {
                    count = available;
                }
                auto *output = static_cast<uint8_t *>(buffer);
                for (uint32_t index = 0U; index < count; ++index) {
                    output[index] = static_cast<uint8_t>(listing[file.offset + index]);
                }
                g_open_files[slot].offset += count;
                return static_cast<int>(count);
            }

            const int bytes = ext2_reader::read_runtime_file(file.ext2_path, file.offset,
                                                             static_cast<uint8_t *>(buffer), count);
            if (bytes < 0) {
                return -1;
            }
            g_open_files[slot].offset += static_cast<uint32_t>(bytes);
            return bytes;
        }

        if (file.file->is_directory) {
            char listing[1024U]{};
            const uint32_t listing_size = build_directory_listing(
                file.file->path, listing, static_cast<uint32_t>(sizeof(listing)));
            if (file.offset >= listing_size) {
                return 0;
            }
            uint32_t available = listing_size - file.offset;
            if (count > available) {
                count = available;
            }
            auto *output = static_cast<uint8_t *>(buffer);
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
        auto *output = static_cast<uint8_t *>(buffer);
        for (uint32_t index = 0U; index < count; ++index) {
            output[index] = file.file->data[file.offset + index];
        }
        g_open_files[slot].offset += count;
        return static_cast<int>(count);
    }

    int write(int fd, const void *buffer, uint32_t count) noexcept {
        if (buffer == nullptr || count == 0U) {
            return 0;
        }
        if (!is_fd_valid(fd)) {
            return -1;
        }

        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }

        OpenFile &file = g_open_files[slot];

        if (file.device_type == DeviceType::DevNull) {
            return static_cast<int>(count); // Discard
        }

        if (file.device_type == DeviceType::DevZero) {
            return static_cast<int>(count); // Discard
        }

        if (file.device_type == DeviceType::VfsDelegate) {
            if (file.vfs_ops != nullptr && file.vfs_ops->write != nullptr) {
                return file.vfs_ops->write(file.vfs_slot, buffer, count);
            }
            return -30; // EROFS
        }

        if (file.is_console) {
            if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_RDONLY) {
                return -1;
            }
            const auto *input = static_cast<const uint8_t *>(buffer);
            for (uint32_t index = 0U; index < count; ++index) {
                xinim::i486::console::tty_write_char(static_cast<char>(input[index]));
            }
            return static_cast<int>(count);
        }
        if (file.is_pipe) {
            Pipe *pipe = get_pipe(file.pipe_id);
            if (pipe == nullptr || pipe->readers == 0U) {
                return -13; // -EPIPE (broken pipe, no readers)
            }
            if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_RDONLY) {
                return -1;
            }
            const size_t available = kPipeCapacity - pipe->count;
            if (count <= kPipeCapacity && static_cast<size_t>(count) > available) {
                if ((file.status_flags & kFileFlagO_NONBLOCK) != 0U) {
                    return -11; // -EAGAIN
                }
                return kWriteWouldBlock;
            }
            const auto *input = static_cast<const uint8_t *>(buffer);
            const uint32_t written = write_pipe_buffer(*pipe, input, count);
            if (written == 0U && count > 0U) {
                if ((file.status_flags & kFileFlagO_NONBLOCK) != 0U) {
                    return -11; // -EAGAIN
                }
                return kWriteWouldBlock; // Caller must block and retry
            }
            return static_cast<int>(written);
        }

        if (file.is_ext2) {
            if (file.ext2_is_directory) {
                return -1;
            }
            if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_RDONLY) {
                return -1;
            }
            if (file.append_mode) {
                g_open_files[slot].offset = g_open_files[slot].ext2_size;
            }
            const int written =
                ext2_reader::write_runtime_file(file.ext2_path, g_open_files[slot].offset,
                                                static_cast<const uint8_t *>(buffer), count);
            if (written < 0) {
                return -1;
            }
            g_open_files[slot].offset += static_cast<uint32_t>(written);
            if (g_open_files[slot].offset > g_open_files[slot].ext2_size) {
                g_open_files[slot].ext2_size = g_open_files[slot].offset;
            }
            return written;
        }

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

        const auto *input = static_cast<const uint8_t *>(buffer);
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
        if (!is_fd_valid(fd)) {
            return -1;
        }

        const size_t slot = fd_to_slot(fd);
        if (!g_open_files[slot].in_use) {
            return fd < kFirstFileDescriptor ? 0 : -1;
        }

        close_open_file_slot(slot);
        return 0;
    }

    bool is_open(int fd) noexcept {
        if (!is_fd_valid(fd)) {
            return false;
        }
        return is_open_file_valid(fd_to_slot(fd));
    }

    bool is_console_fd(int fd) noexcept {
        if (!is_fd_valid(fd)) {
            return false;
        }
        const size_t slot = fd_to_slot(fd);
        return is_open_file_valid(slot) && g_open_files[slot].is_console;
    }

    bool can_read_without_block(int fd) noexcept {
        if (!is_fd_valid(fd)) {
            return false;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return false;
        }
        const OpenFile &file = g_open_files[slot];
        if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_WRONLY) {
            return false;
        }
        if (!file.is_pipe) {
            return true;
        }
        const Pipe *pipe = get_pipe(file.pipe_id);
        return pipe != nullptr && (pipe->count != 0U || pipe->writers == 0U);
    }

    bool can_write_without_block(int fd) noexcept {
        if (!is_fd_valid(fd)) {
            return false;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return false;
        }
        const OpenFile &file = g_open_files[slot];
        if ((file.access & kFileFlagO_ACCMODE) == kFileFlagO_RDONLY) {
            return false;
        }
        if (!file.is_pipe) {
            return true;
        }
        const Pipe *pipe = get_pipe(file.pipe_id);
        return pipe != nullptr && (pipe->readers == 0U || pipe->count < kPipeCapacity);
    }

    int truncate_path(const char *path, uint64_t size) noexcept {
        if (size > UINT32_MAX) {
            return -1;
        }
        char normalized[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, normalized,
                                           static_cast<uint32_t>(sizeof(normalized)), true)) {
            return -1;
        }
        ext2_reader::NodeInfo ext2_info{};
        if (ext2_reader::query_runtime_path(normalized, ext2_info)) {
            return !ext2_info.is_directory && ext2_reader::truncate_runtime_file(
                                                  normalized, static_cast<uint32_t>(size))
                       ? 0
                       : -1;
        }
        FileRecord *file = const_cast<FileRecord *>(find(normalized));
        if (file == nullptr || file->is_directory || file->read_only || size > file->capacity) {
            return -1;
        }
        const uint32_t new_size = static_cast<uint32_t>(size);
        for (uint32_t index = file->size; index < new_size; ++index) {
            file->data[index] = 0U;
        }
        file->size = new_size;
        return 0;
    }

    int truncate_fd(int fd, uint64_t size) noexcept {
        if (!is_fd_valid(fd) || size > UINT32_MAX) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }
        OpenFile &open_file = g_open_files[slot];
        if ((open_file.access & kFileFlagO_ACCMODE) == kFileFlagO_RDONLY || open_file.is_pipe ||
            open_file.is_console) {
            return -1;
        }
        if (open_file.is_ext2) {
            if (open_file.ext2_is_directory ||
                !ext2_reader::truncate_runtime_file(open_file.ext2_path,
                                                    static_cast<uint32_t>(size))) {
                return -1;
            }
            open_file.ext2_size = static_cast<uint32_t>(size);
            return 0;
        }
        if (open_file.file == nullptr || open_file.file->read_only ||
            open_file.file->is_directory || size > open_file.file->capacity) {
            return -1;
        }
        FileRecord &file = *open_file.file;
        const uint32_t new_size = static_cast<uint32_t>(size);
        for (uint32_t index = file.size; index < new_size; ++index) {
            file.data[index] = 0U;
        }
        file.size = new_size;
        return 0;
    }

    int chown_path(const char *path, int64_t user_id, int64_t group_id) noexcept {
        if (user_id < -1 || user_id > UINT32_MAX || group_id < -1 || group_id > UINT32_MAX) {
            return -1;
        }
        char resolved[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, resolved, static_cast<uint32_t>(sizeof(resolved)),
                                           true)) {
            return -1;
        }
        FileRecord *file = const_cast<FileRecord *>(find_normalized_record(resolved));
        if (file == nullptr) {
            return -1;
        }
        if (user_id != -1) {
            file->user_id = static_cast<uint32_t>(user_id);
        }
        if (group_id != -1) {
            file->group_id = static_cast<uint32_t>(group_id);
        }
        return 0;
    }

    int create_symbolic_link(const char *target, const char *link_path) noexcept {
        if (target == nullptr || link_path == nullptr) {
            return -1;
        }
        const uint32_t target_length = string_length(target);
        if (target_length == 0U || target_length >= kTmpPathLength || g_file_count >= kMaxFiles) {
            return -1;
        }

        char resolved_link_path[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(link_path, resolved_link_path,
                                           static_cast<uint32_t>(sizeof(resolved_link_path)),
                                           false)) {
            return -1;
        }
        ext2_reader::NodeInfo existing_ext2_node{};
        if (ext2_reader::query_runtime_path_no_follow(resolved_link_path, existing_ext2_node)) {
            return -1;
        }
        if (ext2_reader::create_symlink_runtime(target, resolved_link_path)) {
            return 0;
        }
        if (!is_tmp_path(resolved_link_path) || !parent_directory_exists(resolved_link_path) ||
            find_normalized_record(resolved_link_path) != nullptr) {
            return -1;
        }

        const uint32_t link_path_length = string_length(resolved_link_path);
        for (size_t slot = 0U; slot < kMaxSymbolicLinks; ++slot) {
            if (g_symbolic_links[slot].in_use) {
                continue;
            }
            SymbolicLinkRecord &storage = g_symbolic_links[slot];
            storage.in_use = true;
            for (uint32_t index = 0U; index <= link_path_length; ++index) {
                storage.path[index] = resolved_link_path[index];
            }
            for (uint32_t index = 0U; index < target_length; ++index) {
                storage.target[index] = static_cast<uint8_t>(target[index]);
            }
            storage.target[target_length] = 0U;

            FileRecord *symbolic_link = allocate_file_record();
            if (symbolic_link == nullptr) {
                zero_object(storage);
                return -1;
            }
            symbolic_link->path = storage.path;
            symbolic_link->data = storage.target;
            symbolic_link->size = target_length;
            symbolic_link->capacity = kTmpPathLength;
            symbolic_link->is_symlink = true;
            reset_path_cache();
            return 0;
        }
        return -1;
    }

    int read_symbolic_link(const char *path, char *buffer, uint32_t capacity) noexcept {
        if (path == nullptr || buffer == nullptr || capacity == 0U) {
            return -1;
        }
        char resolved[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, resolved, static_cast<uint32_t>(sizeof(resolved)),
                                           false)) {
            return -1;
        }
        const int ext2_result = ext2_reader::readlink_runtime(resolved, buffer, capacity);
        if (ext2_result >= 0) {
            return ext2_result;
        }
        const FileRecord *symbolic_link = find_normalized_record(resolved);
        if (symbolic_link == nullptr || !symbolic_link->is_symlink ||
            symbolic_link->data == nullptr) {
            return -1;
        }
        const uint32_t copy_count = symbolic_link->size < capacity ? symbolic_link->size : capacity;
        for (uint32_t index = 0U; index < copy_count; ++index) {
            buffer[index] = static_cast<char>(symbolic_link->data[index]);
        }
        return static_cast<int>(copy_count);
    }

    int chown_fd(int fd, int64_t user_id, int64_t group_id) noexcept {
        if (!is_fd_valid(fd) || user_id < -1 || user_id > UINT32_MAX || group_id < -1 ||
            group_id > UINT32_MAX) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot) || g_open_files[slot].file == nullptr) {
            return -1;
        }
        FileRecord &file = *g_open_files[slot].file;
        if (user_id != -1) {
            file.user_id = static_cast<uint32_t>(user_id);
        }
        if (group_id != -1) {
            file.group_id = static_cast<uint32_t>(group_id);
        }
        return 0;
    }

    int update_file_lock(int fd, uintptr_t owner, int current_mode, int requested_mode) noexcept {
        if (!is_fd_valid(fd) || owner == 0U) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot) || g_open_files[slot].file == nullptr ||
            g_open_files[slot].file->is_directory) {
            return -1;
        }
        FileRecord &file = *g_open_files[slot].file;
        if (requested_mode == 0) {
            if (current_mode == 1 && file.shared_lock_count != 0U) {
                --file.shared_lock_count;
            } else if (current_mode == 2 && file.exclusive_lock_owner == owner) {
                file.exclusive_lock_owner = 0U;
            }
            return 0;
        }
        if (requested_mode == current_mode) {
            return 0;
        }
        if (requested_mode == 1) {
            if (file.exclusive_lock_owner != 0U && file.exclusive_lock_owner != owner) {
                return kLockWouldBlock;
            }
            if (current_mode == 2) {
                file.exclusive_lock_owner = 0U;
            }
            ++file.shared_lock_count;
            return 0;
        }
        if (requested_mode == 2) {
            const uint32_t caller_shared_locks = current_mode == 1 ? 1U : 0U;
            if ((file.exclusive_lock_owner != 0U && file.exclusive_lock_owner != owner) ||
                file.shared_lock_count > caller_shared_locks) {
                return kLockWouldBlock;
            }
            if (current_mode == 1 && file.shared_lock_count != 0U) {
                --file.shared_lock_count;
            }
            file.exclusive_lock_owner = owner;
            return 0;
        }
        return -1;
    }

    int foreground_pgrp() noexcept {
        return g_foreground_pgrp;
    }

    void set_foreground_pgrp(int pgid) noexcept {
        g_foreground_pgrp = pgid;
    }

    bool consume_pipe_eof_event() noexcept {
        if (g_pipe_eof_event) {
            g_pipe_eof_event = false;
            return true;
        }
        return false;
    }

    void increment_slot_refcount(int slot) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= kMaxOpenFiles) {
            return;
        }
        if (g_open_files[slot].in_use) {
            ++g_open_files[slot].refcount;
        }
    }

    void decrement_slot_refcount(int slot) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= kMaxOpenFiles) {
            return;
        }
        if (!g_open_files[slot].in_use) {
            return;
        }
        // Console fds (slots 0/1/2): never close the underlying slot, just decrement
        if (static_cast<size_t>(slot) < kReservedOpenDescriptors) {
            if (g_open_files[slot].refcount > 1U) {
                --g_open_files[slot].refcount;
            }
            return;
        }
        if (g_open_files[slot].refcount > 1U) {
            --g_open_files[slot].refcount;
            return;
        }
        // refcount reached 0: release the slot
        close_open_file_slot(static_cast<size_t>(slot));
    }

    int descriptor_flags_for_slot(int slot) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= kMaxOpenFiles) {
            return -1;
        }
        if (!g_open_files[slot].in_use) {
            return -1;
        }
        return g_open_files[slot].descriptor_flags;
    }

    void increment_pipe_users(int slot) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= kMaxOpenFiles) {
            return;
        }
        if (!g_open_files[slot].in_use || !g_open_files[slot].is_pipe) {
            return;
        }
        Pipe *pipe = get_pipe(g_open_files[slot].pipe_id);
        if (pipe == nullptr) {
            return;
        }
        if (g_open_files[slot].is_pipe_writer) {
            ++pipe->writers;
        } else {
            ++pipe->readers;
        }
    }

    namespace {

        [[nodiscard]] bool copy_parent_directory(const char *path, char *parent,
                                                 uint32_t capacity) noexcept {
            if (path == nullptr || parent == nullptr || capacity < 2U || path[0] != '/' ||
                string_equals(path, "/")) {
                return false;
            }
            uint32_t length = string_length(path);
            uint32_t separator = length;
            while (separator > 0U && path[separator - 1U] != '/') {
                --separator;
            }
            if (separator == 0U) {
                return false;
            }
            const uint32_t parent_length = separator == 1U ? 1U : separator - 1U;
            if (parent_length + 1U > capacity) {
                return false;
            }
            for (uint32_t index = 0U; index < parent_length; ++index) {
                parent[index] = path[index];
            }
            parent[parent_length] = '\0';
            return true;
        }

        [[nodiscard]] bool parent_directory_exists(const char *path) noexcept {
            char parent[kMaxPathLength]{};
            return copy_parent_directory(path, parent, static_cast<uint32_t>(sizeof(parent))) &&
                   is_directory(parent);
        }

        [[nodiscard]] bool path_is_descendant(const char *path, const char *directory) noexcept {
            if (path == nullptr || directory == nullptr || !starts_with(path, directory)) {
                return false;
            }
            const uint32_t directory_length = string_length(directory);
            return path[directory_length] == '/' && path[directory_length + 1U] != '\0';
        }

        [[nodiscard]] bool directory_is_empty(const char *path) noexcept {
            for (uint32_t index = 0U; index < g_file_count; ++index) {
                if (g_files[index].path != nullptr &&
                    path_is_descendant(g_files[index].path, path)) {
                    return false;
                }
            }
            return true;
        }

        char *mutable_record_path(FileRecord &file) noexcept {
            for (size_t slot = 0U; slot < kMaxDynamicFiles; ++slot) {
                if (g_tmp_files[slot].in_use && g_tmp_files[slot].path == file.path) {
                    return g_tmp_files[slot].path;
                }
            }
            for (size_t slot = 0U; slot < kMaxSymbolicLinks; ++slot) {
                if (g_symbolic_links[slot].in_use && g_symbolic_links[slot].path == file.path) {
                    return g_symbolic_links[slot].path;
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool set_mutable_record_path(FileRecord &file, const char *path) noexcept {
            char *storage = mutable_record_path(file);
            const uint32_t length = string_length(path);
            if (storage == nullptr || length + 1U >= kTmpPathLength) {
                return false;
            }
            copy_c_string(storage, kTmpPathLength, path);
            file.path = storage;
            return true;
        }

        void remove_namespace_record(FileRecord &file) noexcept {
            file.path = nullptr;
            reset_path_cache();
            release_unlinked_file_record(&file);
        }

        FileRecord *create_tmp_directory(const char *path) noexcept {
            if (!is_tmp_path(path) || string_equals(path, "/tmp")) {
                return nullptr;
            }
            for (size_t slot = 0U; slot < kMaxDynamicFiles; ++slot) {
                if (g_tmp_files[slot].in_use) {
                    continue;
                }
                const uint32_t length = string_length(path);
                if (length + 1U >= kTmpPathLength) {
                    return nullptr;
                }
                FileRecord *directory = allocate_file_record();
                if (directory == nullptr) {
                    return nullptr;
                }
                RamFile &storage = g_tmp_files[slot];
                storage.in_use = true;
                storage.is_directory = true;
                copy_c_string(storage.path, kTmpPathLength, path);
                directory->path = storage.path;
                directory->data = storage.data;
                directory->is_directory = true;
                reset_path_cache();
                return directory;
            }
            return nullptr;
        }

    } // namespace

    int mkdir(const char *path, uint32_t mode) noexcept {
        char normalized[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, normalized,
                                           static_cast<uint32_t>(sizeof(normalized)), false) ||
            find_normalized_record(normalized) != nullptr) {
            return -1;
        }
        if (ext2_reader::mkdir_runtime_directory(normalized, static_cast<uint16_t>(mode))) {
            return 0;
        }
        return parent_directory_exists(normalized) && create_tmp_directory(normalized) != nullptr
                   ? 0
                   : -1;
    }

    int rename(const char *old_path, const char *new_path) noexcept {
        char normalized_old[kMaxPathLength]{};
        char normalized_new[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(old_path, normalized_old,
                                           static_cast<uint32_t>(sizeof(normalized_old)), false) ||
            !resolve_bootfs_symbolic_links(new_path, normalized_new,
                                           static_cast<uint32_t>(sizeof(normalized_new)), false)) {
            return -1;
        }
        if (string_equals(normalized_old, normalized_new)) {
            return find_normalized_record(normalized_old) != nullptr ? 0 : -1;
        }
        if (ext2_reader::rename_runtime_path(normalized_old, normalized_new)) {
            return 0;
        }

        FileRecord *source = const_cast<FileRecord *>(find_normalized_record(normalized_old));
        FileRecord *destination = const_cast<FileRecord *>(find_normalized_record(normalized_new));
        if (source == nullptr || source->read_only || mutable_record_path(*source) == nullptr ||
            !is_tmp_path(normalized_new) || !parent_directory_exists(normalized_new) ||
            (source->is_directory && path_is_descendant(normalized_new, normalized_old))) {
            return -1;
        }
        if (destination != nullptr) {
            if (destination->read_only || source->is_directory != destination->is_directory ||
                (destination->is_directory && !directory_is_empty(normalized_new))) {
                return -1;
            }
        }

        const uint32_t old_length = string_length(normalized_old);
        const uint32_t new_length = string_length(normalized_new);
        for (uint32_t index = 0U; index < g_file_count; ++index) {
            FileRecord &candidate = g_files[index];
            if (candidate.path == nullptr || !path_is_descendant(candidate.path, normalized_old)) {
                continue;
            }
            const uint32_t suffix_length = string_length(candidate.path + old_length);
            if (new_length + suffix_length + 1U >= kTmpPathLength ||
                mutable_record_path(candidate) == nullptr) {
                return -1;
            }
        }

        if (destination != nullptr) {
            remove_namespace_record(*destination);
        }
        if (!set_mutable_record_path(*source, normalized_new)) {
            return -1;
        }
        for (uint32_t index = 0U; index < g_file_count; ++index) {
            FileRecord &candidate = g_files[index];
            if (candidate.path == nullptr || &candidate == source ||
                !path_is_descendant(candidate.path, normalized_old)) {
                continue;
            }
            char renamed[kMaxPathLength]{};
            copy_c_string(renamed, static_cast<uint32_t>(sizeof(renamed)), normalized_new);
            copy_c_string(renamed + new_length, static_cast<uint32_t>(sizeof(renamed)) - new_length,
                          candidate.path + old_length);
            if (!set_mutable_record_path(candidate, renamed)) {
                return -1;
            }
        }
        reset_path_cache();
        return 0;
    }

    int rmdir(const char *path) noexcept {
        char normalized[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, normalized,
                                           static_cast<uint32_t>(sizeof(normalized)), false)) {
            return -1;
        }
        if (ext2_reader::rmdir_runtime_directory(normalized)) {
            return 0;
        }
        FileRecord *directory = const_cast<FileRecord *>(find_normalized_record(normalized));
        if (directory == nullptr || !directory->is_directory || directory->read_only ||
            !directory_is_empty(normalized)) {
            return -1;
        }
        remove_namespace_record(*directory);
        return 0;
    }

    int unlink(const char *path) noexcept {
        char normalized[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, normalized,
                                           static_cast<uint32_t>(sizeof(normalized)), false)) {
            return -1;
        }
        if (ext2_reader::unlink_runtime_path(normalized)) {
            return 0;
        }
        FileRecord *file = const_cast<FileRecord *>(find_normalized_record(normalized));
        if (file == nullptr || file->is_directory || file->read_only) {
            return -1;
        }
        remove_namespace_record(*file);
        return 0;
    }

    int duplicate(int old_fd, int new_fd) noexcept {
        if (!is_fd_valid(old_fd)) {
            return -1;
        }

        if (new_fd == -1) {
            bool found_slot = false;
            for (uint32_t fd = static_cast<uint32_t>(kFirstFileDescriptor); fd < kMaxOpenFiles;
                 ++fd) {
                if (!g_open_files[fd].in_use) {
                    new_fd = static_cast<int>(fd);
                    found_slot = true;
                    break;
                }
            }
            if (!found_slot) {
                return -1;
            }
        } else if (new_fd < 0) {
            return -1;
        }
        if (!is_fd_valid(new_fd)) {
            return -1;
        }

        const size_t old_slot = fd_to_slot(old_fd);
        if (!is_open_file_valid(old_slot)) {
            return -1;
        }
        if (old_fd == new_fd) {
            return new_fd;
        }

        const size_t new_slot = fd_to_slot(new_fd);
        if (g_open_files[new_slot].in_use) {
            close_open_file_slot(new_slot);
        }

        const OpenFile &source = g_open_files[old_slot];
        g_open_files[new_slot] = source;
        g_open_files[new_slot].descriptor_flags &= ~kFdCloExec;
        if (source.is_pipe) {
            Pipe *pipe = get_pipe(source.pipe_id);
            if (pipe != nullptr) {
                if (source.is_pipe_writer) {
                    ++pipe->writers;
                } else {
                    ++pipe->readers;
                }
            }
        }
        return new_fd;
    }

    int make_pipe(int fd_array[2]) noexcept {
        if (fd_array == nullptr) {
            return -1;
        }
        if (kMaxOpenFiles < 2U) {
            return -1;
        }

        Pipe *pipe = allocate_pipe();
        if (pipe == nullptr) {
            return -1;
        }

        const int read_fd = allocate_open_slot();
        if (read_fd < 0) {
            pipe->in_use = false;
            return -1;
        }
        const size_t read_slot = fd_to_slot(read_fd);
        configure_open_file_entry(read_slot, nullptr, kFileFlagO_RDONLY, false, true, false,
                                  pipe->id, false);
        ++pipe->readers;

        const int write_fd = allocate_open_slot();
        if (write_fd < 0) {
            close_open_file_slot(read_slot);
            pipe->in_use = false;
            return -1;
        }
        const size_t write_slot = fd_to_slot(write_fd);
        configure_open_file_entry(write_slot, nullptr, kFileFlagO_WRONLY, false, true, false,
                                  pipe->id, true);
        ++pipe->writers;

        fd_array[0] = read_fd;
        fd_array[1] = write_fd;
        return 0;
    }

    int access(const char *path) noexcept {
        char resolved[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, resolved, static_cast<uint32_t>(sizeof(resolved)),
                                           true)) {
            return -1;
        }
        if (string_equals(resolved, "/dev/tty") || string_equals(resolved, "/dev/console") ||
            string_equals(resolved, "/dev/null") || string_equals(resolved, "/dev/zero")) {
            return 0;
        }
        ext2_reader::NodeInfo ext2_info{};
        if (ext2_reader::query_runtime_path(resolved, ext2_info)) {
            return 0;
        }
        return find_normalized_record(resolved) != nullptr ? 0 : -1;
    }

    int64_t seek(int fd, int64_t offset, int whence) noexcept {
        if (!is_fd_valid(fd)) {
            return -1;
        }

        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }

        OpenFile &file = g_open_files[slot];
        if (file.is_pipe) {
            return -1;
        }

        uint32_t base = 0U;
        switch (whence) {
        case kSeekSet:
            base = 0U;
            break;
        case kSeekCur:
            base = file.offset;
            break;
        case kSeekEnd:
            if (file.is_ext2) {
                base = file.ext2_size;
            } else {
                base = (file.file != nullptr) ? file.file->size : 0U;
            }
            break;
        default:
            return -1;
        }

        const int64_t next = static_cast<int64_t>(base) + offset;
        if (next < 0) {
            return -1;
        }
        file.offset = static_cast<uint32_t>(next);
        return next;
    }

    void fill_status_record(const FileRecord &file, FileStatus *status) noexcept {
        if (status == nullptr) {
            return;
        }

        *status = {};
        status->mode = file.is_symlink ? 0120000U : (file.is_directory ? 0040000U : 0100000U);
        if (file.read_only) {
            status->mode |= 0444U;
        } else {
            status->mode |= 0644U;
        }
        if (file.executable) {
            status->mode |= 0111U;
        }
        status->link_count = 1U;
        status->user_id = file.user_id;
        status->group_id = file.group_id;
        status->size = file.size;
        status->block_size = 512;
        status->block_count = (file.size + 511U) / 512U;
    }

    void fill_ext2_status_record(const ext2_reader::NodeInfo &info, FileStatus *status) noexcept {
        if (status == nullptr) {
            return;
        }

        *status = {};
        status->mode = info.mode;
        status->link_count = 1U;
        status->size = info.size;
        status->block_size = 512;
        status->block_count = (info.size + 511U) / 512U;
    }

    void fill_device_status_record(FileStatus *status) noexcept {
        if (status == nullptr) {
            return;
        }
        *status = {};
        status->mode = 0020000U | 0666U;
        status->link_count = 1U;
        status->block_size = 512;
    }

    int status_path(const char *path, FileStatus *status) noexcept {
        if (status == nullptr) {
            return -1;
        }
        char resolved[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, resolved, static_cast<uint32_t>(sizeof(resolved)),
                                           true)) {
            return -1;
        }
        if (string_equals(resolved, "/dev/tty") || string_equals(resolved, "/dev/console") ||
            string_equals(resolved, "/dev/null") || string_equals(resolved, "/dev/zero")) {
            fill_device_status_record(status);
            return 0;
        }
        ext2_reader::NodeInfo ext2_info{};
        if (ext2_reader::query_runtime_path(resolved, ext2_info)) {
            fill_ext2_status_record(ext2_info, status);
            return 0;
        }
        const FileRecord *file = find_normalized_record(resolved);
        if (file == nullptr) {
            return -1;
        }
        fill_status_record(*file, status);
        return 0;
    }

    int status_path_no_follow(const char *path, FileStatus *status) noexcept {
        if (status == nullptr) {
            return -1;
        }
        char resolved[kMaxPathLength]{};
        if (!resolve_bootfs_symbolic_links(path, resolved, static_cast<uint32_t>(sizeof(resolved)),
                                           false)) {
            return -1;
        }
        if (string_equals(resolved, "/dev/tty") || string_equals(resolved, "/dev/console") ||
            string_equals(resolved, "/dev/null") || string_equals(resolved, "/dev/zero")) {
            fill_device_status_record(status);
            return 0;
        }
        ext2_reader::NodeInfo ext2_info{};
        if (ext2_reader::query_runtime_path_no_follow(resolved, ext2_info)) {
            fill_ext2_status_record(ext2_info, status);
            return 0;
        }
        const FileRecord *file = find_normalized_record(resolved);
        if (file == nullptr) {
            return -1;
        }
        fill_status_record(*file, status);
        return 0;
    }

    int status_fd(int fd, FileStatus *status) noexcept {
        if (status == nullptr || !is_fd_valid(fd)) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot) || g_open_files[slot].is_pipe) {
            return -1;
        }
        if (g_open_files[slot].is_console || g_open_files[slot].file == nullptr) {
            if (g_open_files[slot].is_ext2) {
                ext2_reader::NodeInfo ext2_info{};
                if (!ext2_reader::query_runtime_path(g_open_files[slot].ext2_path, ext2_info)) {
                    return -1;
                }
                fill_ext2_status_record(ext2_info, status);
                return 0;
            }
            fill_device_status_record(status);
            return 0;
        }
        fill_status_record(*g_open_files[slot].file, status);
        return 0;
    }

    int serialize_legacy_status(const FileStatus &status, UserspaceStat *buffer) noexcept {
        if (buffer == nullptr || status.user_id > UINT16_MAX || status.group_id > UINT16_MAX ||
            status.mode > UINT16_MAX || status.link_count > UINT16_MAX ||
            status.device > UINT32_MAX || status.inode > UINT32_MAX ||
            status.special_device > UINT32_MAX || status.size < 0 ||
            static_cast<uint64_t>(status.size) > UINT32_MAX || status.block_size < 0 ||
            static_cast<uint64_t>(status.block_size) > UINT32_MAX || status.block_count < 0 ||
            static_cast<uint64_t>(status.block_count) > UINT32_MAX) {
            return -1;
        }
        *buffer = {};
        buffer->st_dev = static_cast<uint32_t>(status.device);
        buffer->st_ino = static_cast<uint32_t>(status.inode);
        buffer->st_mode = static_cast<uint16_t>(status.mode);
        buffer->st_nlink = static_cast<uint16_t>(status.link_count);
        buffer->st_uid = static_cast<uint16_t>(status.user_id);
        buffer->st_gid = static_cast<uint16_t>(status.group_id);
        buffer->st_rdev = static_cast<uint32_t>(status.special_device);
        buffer->st_size = static_cast<uint32_t>(status.size);
        buffer->st_blksize = static_cast<uint32_t>(status.block_size);
        buffer->st_blocks = static_cast<uint32_t>(status.block_count);
        buffer->st_atime = static_cast<int32_t>(status.access_time);
        buffer->st_atime_nsec = static_cast<uint32_t>(status.access_time_nanoseconds);
        buffer->st_mtime = static_cast<int32_t>(status.modification_time);
        buffer->st_mtime_nsec = static_cast<uint32_t>(status.modification_time_nanoseconds);
        buffer->st_ctime = static_cast<int32_t>(status.status_change_time);
        buffer->st_ctime_nsec = static_cast<uint32_t>(status.status_change_time_nanoseconds);
        return 0;
    }

    int stat_path(const char *path, UserspaceStat *buffer) noexcept {
        FileStatus status{};
        return status_path(path, &status) == 0 ? serialize_legacy_status(status, buffer) : -1;
    }

    int stat_fd(int fd, UserspaceStat *buffer) noexcept {
        FileStatus status{};
        return status_fd(fd, &status) == 0 ? serialize_legacy_status(status, buffer) : -1;
    }

    int descriptor_flags(int fd) noexcept {
        if (!is_fd_valid(fd)) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }
        return g_open_files[slot].descriptor_flags;
    }

    int set_descriptor_flags(int fd, int flags) noexcept {
        if (!is_fd_valid(fd)) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }
        g_open_files[slot].descriptor_flags = flags & kFdCloExec;
        return 0;
    }

    int status_flags(int fd) noexcept {
        if (!is_fd_valid(fd)) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }
        return static_cast<int>(g_open_files[slot].status_flags);
    }

    int set_status_flags(int fd, int flags) noexcept {
        if (!is_fd_valid(fd)) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot)) {
            return -1;
        }
        OpenFile &file = g_open_files[slot];
        const uint32_t preserve_access = file.access & kFileFlagO_ACCMODE;
        file.status_flags = preserve_access | (static_cast<uint32_t>(flags) &
                                               (kFileFlagO_APPEND | kFileFlagO_NONBLOCK));
        file.append_mode = (file.status_flags & kFileFlagO_APPEND) != 0U;
        return 0;
    }

    int control(int fd, int command, uintptr_t argument) noexcept {
        if (!is_fd_valid(fd)) {
            return -1;
        }
        const size_t slot = fd_to_slot(fd);
        if (!is_open_file_valid(slot) || !g_open_files[slot].is_console) {
            return -1;
        }

        switch (static_cast<unsigned long>(command)) {
        case kTermiosGet: {
            auto *state = reinterpret_cast<xinim::i486::tty::TermiosState *>(argument);
            if (state == nullptr) {
                return -1;
            }
            xinim::i486::tty::get_termios(state);
            return 0;
        }
        case kTermiosSetNow:
        case kTermiosSetDrain:
        case kTermiosSetFlush: {
            const auto *state = reinterpret_cast<const xinim::i486::tty::TermiosState *>(argument);
            if (state == nullptr) {
                return -1;
            }
            xinim::i486::tty::set_termios(state);
            return 0;
        }
        case kWindowSizeGet: {
            auto *ws = reinterpret_cast<xinim::i486::tty::WindowSize *>(argument);
            if (ws == nullptr) {
                return -1;
            }
            xinim::i486::tty::get_winsize(ws);
            return 0;
        }
        case kWindowSizeSet: {
            const auto *ws = reinterpret_cast<const xinim::i486::tty::WindowSize *>(argument);
            if (ws == nullptr) {
                return -1;
            }
            xinim::i486::tty::set_winsize(ws);
            return 0;
        }
        case kTtyGetPgrp: {
            auto *pgrp = reinterpret_cast<int *>(argument);
            if (pgrp == nullptr) {
                return -1;
            }
            *pgrp = g_foreground_pgrp;
            return 0;
        }
        case kTtySetPgrp: {
            const auto *pgrp = reinterpret_cast<const int *>(argument);
            if (pgrp == nullptr) {
                return -1;
            }
            g_foreground_pgrp = *pgrp;
            return 0;
        }
        default:
            return -1;
        }
    }

} // namespace xinim::kernel::bootfs

#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

extern "C" void* memset(void* destination, int value, unsigned int count) noexcept {
    auto* bytes = static_cast<unsigned char*>(destination);
    for (unsigned int index = 0U; index < count; ++index) {
        bytes[index] = static_cast<unsigned char>(value);
    }
    return destination;
}

namespace {

constexpr char kMissingPath[] = "ls: inaccessible or not found\r\n";
constexpr char kReadFail[] = "ls: read failed\r\n";
constexpr char kUsage[] = "usage: ls [-l] [path ...]\r\n";
constexpr uint32_t kIoBufferSize = 256U;
constexpr uint32_t kEntryBufferSize = 128U;
constexpr uint32_t kPathBufferSize = 160U;
constexpr uint32_t kErrorThreshold = 0xFFFFF000U;

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    uint32_t length = 0U;
    while (text != nullptr && text[length] != '\0') {
        ++length;
    }
    return length;
}

[[nodiscard]] bool is_error(uint32_t result) noexcept {
    return result >= kErrorThreshold;
}

void write_bytes(int fd, const char* text, uint32_t length) noexcept {
    static_cast<void>(xinim::userland::i386::write(fd, text, length));
}

void write_string(int fd, const char* text) noexcept {
    write_bytes(fd, text, string_length(text));
}

void write_u32(int fd, uint32_t value) noexcept {
    char digits[16]{};
    uint32_t length = 0U;
    if (value == 0U) {
        digits[length++] = '0';
    } else {
        while (value != 0U && length < sizeof(digits)) {
            digits[length++] = static_cast<char>('0' + (value % 10U));
            value /= 10U;
        }
    }
    while (length > 0U) {
        --length;
        write_bytes(fd, digits + length, 1U);
    }
}

[[nodiscard]] const char* base_name(const char* path) noexcept {
    if (path == nullptr || path[0] == '\0') {
        return "";
    }
    const char* base = path;
    for (const char* cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' && cursor[1] != '\0') {
            base = cursor + 1;
        }
    }
    return base;
}

[[nodiscard]] bool is_directory_mode(uint16_t mode) noexcept {
    return (mode & xinim::userland::kStatTypeMask) == xinim::userland::kStatDirectory;
}

void print_long_entry(const char* display_name,
                      bool is_directory,
                      uint32_t size) noexcept {
    const char type = is_directory ? 'd' : '-';
    write_bytes(1, &type, 1U);
    write_string(1, " ");
    write_u32(1, size);
    write_string(1, " ");
    write_string(1, display_name);
    write_string(1, "\r\n");
}

void print_short_entry(const char* display_name) noexcept {
    write_string(1, display_name);
    write_string(1, "\r\n");
}

bool join_child_path(const char* directory,
                     const char* entry_name,
                     char* output,
                     uint32_t capacity) noexcept {
    if (directory == nullptr || entry_name == nullptr || output == nullptr || capacity < 2U) {
        return false;
    }

    uint32_t out = 0U;
    if (directory[0] == '/' && directory[1] == '\0') {
        output[out++] = '/';
    } else {
        for (uint32_t i = 0U; directory[i] != '\0'; ++i) {
            if (out + 1U >= capacity) {
                return false;
            }
            output[out++] = directory[i];
        }
        if (out == 0U || output[out - 1U] != '/') {
            if (out + 1U >= capacity) {
                return false;
            }
            output[out++] = '/';
        }
    }

    for (uint32_t i = 0U; entry_name[i] != '\0'; ++i) {
        if (entry_name[i] == '/' && entry_name[i + 1U] == '\0') {
            break;
        }
        if (out + 1U >= capacity) {
            return false;
        }
        output[out++] = entry_name[i];
    }
    output[out] = '\0';
    return true;
}

void emit_directory_entry(const char* parent_path,
                          const char* entry_name,
                          bool long_format) noexcept {
    if (!long_format) {
        print_short_entry(entry_name);
        return;
    }

    char child_path[kPathBufferSize]{};
    if (!join_child_path(parent_path, entry_name, child_path, sizeof(child_path))) {
        write_string(2, kReadFail);
        return;
    }

    xinim::userland::UserspaceStat stat_buffer{};
    const uint32_t stat_result = xinim::userland::i386::stat(child_path, &stat_buffer);
    if (is_error(stat_result)) {
        write_string(2, kReadFail);
        return;
    }

    print_long_entry(entry_name, is_directory_mode(stat_buffer.st_mode), stat_buffer.st_size);
}

int list_directory(const char* path,
                   bool long_format,
                   bool print_header) noexcept {
    const uint32_t fd = xinim::userland::i386::open(path, 0U, 0U);
    if (is_error(fd)) {
        write_string(2, kMissingPath);
        return 1;
    }

    if (print_header) {
        write_string(1, path);
        write_string(1, ":\r\n");
    }

    char io_buffer[kIoBufferSize]{};
    char entry[kEntryBufferSize]{};
    uint32_t entry_length = 0U;

    for (;;) {
        const uint32_t result = xinim::userland::i386::read(static_cast<int>(fd), io_buffer, sizeof(io_buffer));
        if (result == 0U) {
            break;
        }
        if (is_error(result)) {
            static_cast<void>(xinim::userland::i386::close(static_cast<int>(fd)));
            write_string(2, kReadFail);
            return 1;
        }

        for (uint32_t index = 0U; index < result; ++index) {
            const char value = io_buffer[index];
            if (value == '\n') {
                entry[entry_length] = '\0';
                if (entry_length != 0U) {
                    emit_directory_entry(path, entry, long_format);
                }
                entry_length = 0U;
                continue;
            }
            if (value == '\r') {
                continue;
            }
            if (entry_length + 1U < sizeof(entry)) {
                entry[entry_length++] = value;
            }
        }
    }

    if (entry_length != 0U) {
        entry[entry_length] = '\0';
        emit_directory_entry(path, entry, long_format);
    }

    static_cast<void>(xinim::userland::i386::close(static_cast<int>(fd)));
    if (print_header) {
        write_string(1, "\r\n");
    }
    return 0;
}

int list_path(const char* path,
              bool long_format,
              bool print_header) noexcept {
    xinim::userland::UserspaceStat stat_buffer{};
    const uint32_t stat_result = xinim::userland::i386::stat(path, &stat_buffer);
    if (is_error(stat_result)) {
        write_string(2, kMissingPath);
        return 1;
    }

    if (is_directory_mode(stat_buffer.st_mode)) {
        return list_directory(path, long_format, print_header);
    }

    const char* display_name = print_header ? base_name(path) : path;
    if (long_format) {
        print_long_entry(display_name, false, stat_buffer.st_size);
    } else {
        print_short_entry(display_name);
    }
    return 0;
}

} // namespace

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    (void)envp;

    bool long_format = false;
    int first_path = 1;
    while (first_path < argc && argv != nullptr && argv[first_path] != nullptr &&
           argv[first_path][0] == '-') {
        if (argv[first_path][1] == 'l' && argv[first_path][2] == '\0') {
            long_format = true;
            ++first_path;
            continue;
        }
        write_string(2, kUsage);
        return 1;
    }

    if (argc <= first_path || argv == nullptr || argv[first_path] == nullptr) {
        return list_path(".", long_format, false);
    }

    int status = 0;
    const bool multiple_paths = (argc - first_path) > 1;
    for (int index = first_path; index < argc; ++index) {
        if (argv[index] == nullptr) {
            continue;
        }
        const int result = list_path(argv[index], long_format, multiple_paths);
        if (result != 0) {
            status = result;
        }
    }
    return status;
}

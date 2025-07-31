/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "minix/io/stdio_compat.hpp"
#include "minix/io/file_stream.hpp"
#include "minix/io/standard_streams.hpp"
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace minix::io::compat {

static std::unordered_map<FILE *, Stream *> file_to_stream_map;
static std::mutex map_mutex;

void register_file_stream(FILE *file, Stream *stream) {
    std::lock_guard<std::mutex> lock(map_mutex);
    file_to_stream_map[file] = stream;
}

Stream *get_stream(FILE *file) {
    if (file == stdin)
        return &minix::io::stdin();
    if (file == stdout)
        return &minix::io::stdout();
    if (file == stderr)
        return &minix::io::stderr();
    std::lock_guard<std::mutex> lock(map_mutex);
    auto it = file_to_stream_map.find(file);
    return it != file_to_stream_map.end() ? it->second : nullptr;
}

extern "C" {

FILE *fopen_compat(const char *path, const char *mode) {
    bool read = false, write = false, append = false;
    OpenMode open_mode{};
    for (const char *p = mode; *p; ++p) {
        switch (*p) {
        case 'r':
            read = true;
            break;
        case 'w':
            write = true;
            open_mode = open_mode | OpenMode::create | OpenMode::truncate;
            break;
        case 'a':
            append = true;
            write = true;
            open_mode = open_mode | OpenMode::append;
            break;
        case '+':
            read = write = true;
            break;
        default:
            break;
        }
    }
    if (read && write) {
        open_mode = open_mode | OpenMode::read | OpenMode::write;
    } else if (read) {
        open_mode = open_mode | OpenMode::read;
    } else if (write) {
        open_mode = open_mode | OpenMode::write;
    }
    auto result = open_stream(path, open_mode);
    if (!result)
        return nullptr;
    FILE *fake = new FILE{};
    fake->_fd = (*result.value)->descriptor();
    fake->_flags = (read ? READMODE : 0) | (write ? WRITEMODE : 0);
    register_file_stream(fake, result.value->get());
    return fake;
}

int fclose_compat(FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return EOF;
    auto err = stream->close();
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        file_to_stream_map.erase(fp);
    }
    if (fp != stdin && fp != stdout && fp != stderr) {
        delete fp;
    }
    return err ? EOF : 0;
}

size_t fread_compat(void *ptr, size_t size, size_t nmemb, FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return 0;
    size_t bytes = size * nmemb;
    auto result = stream->read(std::span<std::byte>(static_cast<std::byte *>(ptr), bytes));
    return result ? (*result.value / size) : 0;
}

size_t fwrite_compat(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return 0;
    size_t bytes = size * nmemb;
    auto result =
        stream->write(std::span<const std::byte>(static_cast<const std::byte *>(ptr), bytes));
    return result ? (*result.value / size) : 0;
}

int fprintf_compat(FILE *fp, const char *format, ...) {
    auto *stream = get_stream(fp);
    if (!stream)
        return -1;
    va_list args;
    va_start(args, format);
    // simple formatted print using vsnprintf then write
    char buffer[256];
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n < 0)
        return -1;
    auto wr = stream->write(std::span<const std::byte>(reinterpret_cast<const std::byte *>(buffer),
                                                       static_cast<size_t>(n)));
    return wr ? n : -1;
}

} // extern "C"

} // namespace minix::io::compat

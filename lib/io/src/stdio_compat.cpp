#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <expected>
#include <mutex>
#include <span>
#include <system_error>
#include <unordered_map>
#include "minix/io/file_stream.hpp"
#include "minix/io/standard_streams.hpp"
#include "minix/io/file_operations.hpp"
#include "minix/io/stdio_compat.hpp"

namespace minix::io::compat {

/// Map tracking FILE* handles to their owning Stream objects.
static std::unordered_map<FILE *, Stream *> file_to_stream_map;
/// Guards modifications to file_to_stream_map.
static std::mutex map_mutex;

/// Associate a stdio FILE pointer with a Stream object.
///
/// \param file   The FILE* handle.
/// \param stream Stream instance managing the descriptor.
void register_file_stream(FILE *file, Stream *stream) {
    std::lock_guard<std::mutex> lock(map_mutex);
    file_to_stream_map[file] = stream;
}

/// Retrieve the Stream associated with a stdio FILE pointer.
///
/// \param file The FILE* to look up.
/// \return     Pointer to the corresponding Stream or nullptr.
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

/// fopen replacement using the Stream API.
///
/// \param path Path to open.
/// \param mode Mode string as accepted by fopen.
/// \return     Newly allocated FILE pointer or nullptr on error.
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
    fake->_fd = (*result)->descriptor();
    fake->_flags = (read ? READMODE : 0) | (write ? WRITEMODE : 0);
    register_file_stream(fake, result->get());
    return fake;
}

/// fclose replacement for Stream backed FILE pointers.
///
/// \param fp FILE pointer returned by fopen_compat.
/// \return   0 on success or EOF on error.
int fclose_compat(FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return STDIO_EOF;
    auto err = stream->close();
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        file_to_stream_map.erase(fp);
    }
    if (fp != stdin && fp != stdout && fp != stderr) {
        delete fp;
    }
    return err ? STDIO_EOF : 0;
}

/// fread replacement using Stream::read.
///
/// \param ptr   Destination buffer.
/// \param size  Size of each element.
/// \param nmemb Number of elements to read.
/// \param fp    FILE pointer to read from.
/// \return      Number of elements successfully read.
size_t fread_compat(void *ptr, size_t size, size_t nmemb, FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return 0;
    size_t bytes = size * nmemb;
    auto result = stream->read(std::span<std::byte>(static_cast<std::byte *>(ptr), bytes));
    if (result.has_value()) {
        return result.value() / size;
    } else {
        errno = result.error().value(); // Set errno from error_code
        return 0;
    }
}

/// fwrite replacement using Stream::write.
///
/// \param ptr   Source buffer.
/// \param size  Size of each element.
/// \param nmemb Number of elements to write.
/// \param fp    FILE pointer to write to.
/// \return      Number of elements successfully written.
size_t fwrite_compat(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return 0;
    size_t bytes = size * nmemb;
    auto result =
        stream->write(std::span<const std::byte>(static_cast<const std::byte *>(ptr), bytes));
    if (result.has_value()) {
        return result.value() / size;
    } else {
        errno = result.error().value(); // Set errno from error_code
        return 0;
    }
}

/// fprintf replacement using the Stream API.
///
/// \param fp     FILE pointer to write to.
/// \param format printf style format string.
/// \return       Number of characters written or negative on error.
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
        return -1; // vsnprintf error
    auto wr_result = stream->write(std::span<const std::byte>(
        reinterpret_cast<const std::byte *>(buffer), static_cast<size_t>(n)));
    if (wr_result.has_value()) {
        return n; // Return number of characters successfully formatted by vsnprintf
    } else {
        errno = wr_result.error().value(); // Set errno from error_code
        return -1;
    }
}

} // extern "C"

} // namespace minix::io::compat

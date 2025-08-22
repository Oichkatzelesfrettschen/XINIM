#include "xinim/io/stdio_compat.hpp"

/**
 * @file stdio_compat.cpp
 * @brief Implementation of C stdio shims using `std::fstream` and `std::span`.
 */

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>

namespace minix::io::compat {

/// Map tracking FILE* handles to their backing `std::fstream` instances.
static std::unordered_map<FILE *, std::unique_ptr<std::fstream>> file_to_stream_map;
/// Guards modifications to file_to_stream_map.
static std::mutex map_mutex;

/**
 * @brief Retrieve the `std::fstream` associated with a stdio FILE pointer.
 *
 * @param file The FILE* to look up.
 * @return Pointer to the corresponding `std::fstream` or nullptr.
 */
static std::fstream *get_stream(FILE *file) {
    std::lock_guard<std::mutex> lock(map_mutex);
    auto it = file_to_stream_map.find(file);
    return it != file_to_stream_map.end() ? it->second.get() : nullptr;
}

extern "C" {

/**
 * @brief fopen replacement implemented with `std::fstream`.
 *
 * @param path Path to open.
 * @param mode Mode string as accepted by `fopen`.
 * @return Newly allocated FILE pointer or nullptr on error.
 */
FILE *fopen_compat(const char *path, const char *mode) {
    std::ios_base::openmode open_mode{};
    for (const char *p = mode; *p; ++p) {
        switch (*p) {
        case 'r':
            open_mode |= std::ios::in;
            break;
        case 'w':
            open_mode |= std::ios::out | std::ios::trunc;
            break;
        case 'a':
            open_mode |= std::ios::out | std::ios::app;
            break;
        case '+':
            open_mode |= std::ios::in | std::ios::out;
            break;
        default:
            break;
        }
    }
    auto stream = std::make_unique<std::fstream>();
    stream->open(path, open_mode);
    if (!*stream)
        return nullptr;
    FILE *fake = new FILE{};
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        file_to_stream_map[fake] = std::move(stream);
    }
    return fake;
}

/**
 * @brief fclose replacement for `fopen_compat` handles.
 *
 * @param fp FILE pointer returned by `fopen_compat`.
 * @return 0 on success or EOF on error.
 */
int fclose_compat(FILE *fp) {
    std::unique_ptr<std::fstream> stream;
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        auto it = file_to_stream_map.find(fp);
        if (it == file_to_stream_map.end())
            return STDIO_EOF;
        stream = std::move(it->second);
        file_to_stream_map.erase(it);
    }
    stream->close();
    delete fp;
    return 0;
}

/**
 * @brief fread replacement using `std::fstream::read` and `std::span`.
 *
 * @param ptr   Destination buffer.
 * @param size  Size of each element.
 * @param nmemb Number of elements to read.
 * @param fp    FILE pointer to read from.
 * @return Number of elements successfully read.
 */
size_t fread_compat(void *ptr, size_t size, size_t nmemb, FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return 0;
    std::span<std::byte> buffer{static_cast<std::byte *>(ptr), size * nmemb};
    stream->read(reinterpret_cast<char *>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));
    return static_cast<size_t>(stream->gcount()) / size;
}

/**
 * @brief fwrite replacement using `std::fstream::write` and `std::span`.
 *
 * @param ptr   Source buffer.
 * @param size  Size of each element.
 * @param nmemb Number of elements to write.
 * @param fp    FILE pointer to write to.
 * @return Number of elements successfully written.
 */
size_t fwrite_compat(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
    auto *stream = get_stream(fp);
    if (!stream)
        return 0;
    std::span<const std::byte> buffer{static_cast<const std::byte *>(ptr), size * nmemb};
    stream->write(reinterpret_cast<const char *>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));
    if (!*stream)
        return 0;
    return nmemb;
}

/**
 * @brief fprintf replacement using the `std::fstream` API.
 *
 * @param fp     FILE pointer to write to.
 * @param format printf style format string.
 * @return Number of characters written or negative on error.
 */
int fprintf_compat(FILE *fp, const char *format, ...) {
    auto *stream = get_stream(fp);
    if (!stream)
        return -1;
    va_list args;
    va_start(args, format);
    char buffer[256];
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n < 0)
        return -1;
    stream->write(buffer, n);
    if (!*stream)
        return -1;
    return n;
}

} // extern "C"

} // namespace minix::io::compat

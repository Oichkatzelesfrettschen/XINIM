// clang-format off
#include "../include/stdio.hpp"
// clang-format on

/**
 * @brief Write elements from buffer to the specified stream.
 *
 * @param ptr   Source data buffer.
 * @param size  Size of each element.
 * @param count Number of elements to write.
 * @param file  Destination stream.
 * @return Number of elements successfully written.
 */
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *file) {
    const char *bytes = static_cast<const char *>(ptr);
    size_t total = size * count;
    for (size_t i = 0; i < total; ++i) {
        if (putc(bytes[i], file) == STDIO_EOF)
            return i / size;
    }
    return count;
}

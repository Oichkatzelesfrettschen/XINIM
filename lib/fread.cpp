// clang-format off
#include "../include/stdio.hpp"
// clang-format on

/**
 * @brief Read elements from a stream into the provided buffer.
 *
 * @param ptr   Destination buffer.
 * @param size  Size of each element.
 * @param count Number of elements to read.
 * @param file  Source stream.
 * @return Number of elements successfully read.
 * @sideeffects Consumes bytes from @p file and writes to @p ptr.
 * @thread_safety Relies on stdio synchronization.
 * @compat fread(3)
 * @example
 * fread(buf,1,10,file);
 */
size_t fread(void *ptr, size_t size, size_t count, FILE *file) {
    auto *bytes = static_cast<char *>(ptr);
    size_t total = size * count;
    size_t read_bytes = 0;
    while (read_bytes < total) {
        int c = getc(file);
        if (c == STDIO_EOF)
            break;
        bytes[read_bytes++] = static_cast<char>(c);
    }
    return read_bytes / size;
}

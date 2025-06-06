// clang-format off
#include <cstdio>
#include "../include/stdio.hpp"
// clang-format on

// Write elements from buffer to the specified stream.
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *file) {
    const char *bytes = static_cast<const char *>(ptr);
    size_t total = size * count;
    for (size_t i = 0; i < total; ++i) {
        if (putc(bytes[i], file) == EOF)
            return i / size;
    }
    return count;
}

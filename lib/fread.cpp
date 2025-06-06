// clang-format off
#include <cstdio>
#include "../include/stdio.hpp"
// clang-format on

// Read elements from a stream into the provided buffer.
size_t fread(void *ptr, size_t size, size_t count, FILE *file) {
    auto *bytes = static_cast<char *>(ptr);
    size_t total = size * count;
    size_t read_bytes = 0;
    while (read_bytes < total) {
        int c = getc(file);
        if (c == EOF)
            break;
        bytes[read_bytes++] = static_cast<char>(c);
    }
    return read_bytes / size;
}

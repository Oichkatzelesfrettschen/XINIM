// clang-format off
#include <cstdio>
#include "../include/stdio.hpp"
// clang-format on

// Write a string to the specified file stream.
int fputs(const char *s, FILE *file) {
    while (*s != '\0') {
        putc(*s++, file);
    }
    return 0;
}

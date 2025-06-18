// clang-format off
#include "../include/stdio.hpp"
// clang-format on

/**
 * @brief Write a string to the specified file stream.
 *
 * @param s    Null-terminated string to write.
 * @param file Target stream.
 * @return Always 0 on success.
 */
int fputs(const char *s, FILE *file) {
    while (*s != '\0') {
        putc(*s++, file);
    }
    return 0;
}

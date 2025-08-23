// clang-format off
#include "../include/stdio.hpp"
// clang-format on

/**
 * @brief Write a string to the specified file stream.
 *
 * @param s    Null-terminated string to write.
 * @param file Target stream.
 * @return Always 0 on success.
 * @sideeffects Writes characters to @p file.
 * @thread_safety Relies on stdio locks; guard external FILE* if shared.
 * @compat fputs(3)
 * @example
 * fputs("hello", stdout);
 */
int fputs(const char *s, FILE *file) {
    while (*s != '\0') {
        putc(*s++, file);
    }
    return 0;
}

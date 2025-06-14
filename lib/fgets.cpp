#include "../include/stdio.hpp"

/**
 * @brief Read a line from a stream.
 *
 * @param str  Buffer to store the characters.
 * @param n    Maximum number of characters to read including the terminator.
 * @param file Input stream.
 * @return @p str on success or @c nullptr on failure/end of file.
 */
char *fgets(char *str, unsigned n, FILE *file) {
    int ch;
    char *ptr = str;
    while (--n > 0 && (ch = getc(file)) != STDIO_EOF) {
        *ptr++ = ch;
        if (ch == '\n')
            break;
    }
    if (ch == STDIO_EOF && ptr == str)
        return nullptr;
    *ptr = '\0';
    return str;
}

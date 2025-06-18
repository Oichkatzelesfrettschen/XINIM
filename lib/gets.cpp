#include "../include/stdio.hpp"

/**
 * @brief Read a line from @a stdin into the provided buffer.
 *
 * This is a direct translation of the historical \c gets implementation.
 * The function stores characters until a newline or EOF is encountered.
 * It is inherently unsafe and exists purely for compatibility with the
 * original MINIX utilities.
 *
 * @param str Destination buffer to populate.
 * @return Pointer to @p str on success or nullptr on failure.
 */
char *gets(char *str) {
    int ch;
    char *ptr = str;

    while ((ch = getc(stdin)) != STDIO_EOF && ch != '\n') {
        *ptr++ = static_cast<char>(ch);
    }

    if (ch == STDIO_EOF && ptr == str) {
        return nullptr;
    }

    *ptr = '\0';
    return str;
}

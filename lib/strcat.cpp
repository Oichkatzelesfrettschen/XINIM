#include <cstring>

/**
 * @brief Append one C-string to another.
 *
 * @param s1 Destination buffer containing a null-terminated string.
 * @param s2 Source string to append.
 * @return Pointer to the destination buffer.
 */
char *strcat(char *s1, const char *s2) {
    char *original = s1;
    while (*s1 != 0)
        s1++;
    while (*s2 != 0)
        *s1++ = *s2++;
    *s1 = 0;
    return original;
}

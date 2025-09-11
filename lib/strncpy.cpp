#include <cstring>

/**
 * @brief Copy at most n characters from one C-string to another.
 *
 * @sideeffects Writes to @p s1.
 * @thread_safety Caller must synchronize access to @p s1.
 * @compat strncpy(3)
 * @example
 * char dst[8];
 * strncpy(dst, "hello", 8);
 */
char *strncpy(char *s1, const char *s2, int n) {
    char *original = s1;
    while (*s2 != 0) {
        *s1++ = *s2++;
        if (--n == 0)
            break;
    }
    *s1 = 0;
    return original;
}

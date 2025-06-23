#include <cstring>

/**
 * @brief Copy at most n characters from one C-string to another.
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

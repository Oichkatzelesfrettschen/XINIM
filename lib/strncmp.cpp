#include <cstring>

/**
 * @brief Compare two strings up to n characters.
 */
int strncmp(const char *s1, const char *s2, int n) {
    while (n-- > 0) {
        if (*s1 != *s2)
            return (*s1 - *s2);
        if (*s1 == 0)
            return 0;
        ++s1;
        ++s2;
    }
    return 0;
}

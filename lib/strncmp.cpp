#include <cstring>

/**
 * @brief Compare two strings up to n characters.
 *
 * @sideeffects None.
 * @thread_safety Safe for concurrent use; operates on provided buffers only.
 * @compat strncmp(3)
 * @example
 * if (strncmp("abC", "abc", 2) == 0) {
 *     // First two characters match
 * }
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

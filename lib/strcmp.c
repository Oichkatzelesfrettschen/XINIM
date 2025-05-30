/* Compare two strings and return the difference between the first
 * differing characters or zero if they match. */

#include <string.h>

/* Lexicographically compare s1 and s2. */
int strcmp(const char *s1, const char *s2) {
    while (1) {
        if (*s1 != *s2)
            return (*s1 - *s2); /* return difference when chars differ */
        if (*s1 == '\0')
            return 0; /* reached end of both strings */
        s1++;
        s2++;
    }
}

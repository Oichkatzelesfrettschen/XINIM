/* Compare two strings and return the difference between the first
 * differing characters or zero if they match. */

#include <cstring>

/* Lexicographically compare s1 and s2. */
int strcmp(char *s1, char *s2) {
    while (1) {
        if (*s1 != *s2)
            return (*s1 - *s2); /* return difference when chars differ */
        if (*s1 == '\0')
            return 0; /* reached end of both strings */
        s1++;
        s2++;
    }
}

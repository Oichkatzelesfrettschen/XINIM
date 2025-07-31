/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Compare two strings and return the difference between the first
 * differing characters or zero if they match. */

#include <string.h>

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

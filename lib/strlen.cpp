/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Compute the length of a string. */

#include <string.h>

/* Return the number of characters in the string s. */
int strlen(char *s) {
    const char *original = s; /* keep starting position */

    /* Walk forward until the terminating null byte. */
    while (*s != '\0') {
        s++;
    }

    /* Difference gives the length. */
    return static_cast<int>(s - original);
}

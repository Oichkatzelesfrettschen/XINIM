/* Compute the length of a string. */

#include <cstring>

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

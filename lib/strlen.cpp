/* Compute the length of a string. */

#include <string.h>

/* Return the number of characters in the string s. */
size_t strlen(const char *s) {
    const char *original = s; /* keep starting position */

    /* Walk forward until the terminating null byte. */
    while (*s != '\0') {
        s++;
    }

    /* Difference gives the length. */
    return (size_t)(s - original);
}

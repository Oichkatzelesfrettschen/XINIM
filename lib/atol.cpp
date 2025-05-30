#include <cctype> // for std::isspace
#include <cstdlib>

/* Convert a string to a long integer. */
long atol(char *s) {
    long total = 0; /* running total */
    unsigned digit; /* current digit */
    int minus = 0;  /* leading '-' seen */

    /* Skip whitespace characters. */
    while (std::isspace(static_cast<unsigned char>(*s)))
        s++;

    /* Check for a sign prefix. */
    if (*s == '-') {
        s++;
        minus = 1;
    }

    /* Accumulate decimal digits. */
    while ((digit = *s++ - '0') < 10) {
        total *= 10;
        total += digit;
    }

    return minus ? -total : total;
}

/* Implementation of the standard atoi() function. */

#include <cctype> // for std::isspace and friends
#include <cstdlib>

/* Convert a numeric string to an integer. */
int atoi(const char *s) {
    int total = 0;  /* running total */
    unsigned digit; /* current digit */
    int minus = 0;  /* track a leading '-' */

    /* Skip leading whitespace characters. */
    while (std::isspace(static_cast<unsigned char>(*s))) {
        s++;
    }

    /* Handle optional leading minus sign. */
    if (*s == '-') {
        s++;
        minus = 1;
    }

    /* Process digits until a non-digit is encountered. */
    while ((digit = *s++ - '0') < 10) {
        total *= 10;
        total += digit;
    }

    return minus ? -total : total;
}

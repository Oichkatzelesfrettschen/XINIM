/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Implementation of the standard atoi() function. */

#include "../include/number_parse.hpp" // shared decimal parser

/* Convert a numeric string to an int using the shared parser. */
int atoi(const char *s) {
    // Use the common helper to parse a signed decimal value and
    // narrow the result to an int for backwards compatibility.
    return static_cast<int>(parse_signed_decimal(s));
}

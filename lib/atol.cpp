#include "../include/number_parse.hpp" // shared decimal parser

/* Convert a string to a long integer using the common parser. */
long atol(char *s) {
    // The historical prototype expects a mutable char pointer, so
    // simply pass it to the helper which takes a const pointer.
    return parse_signed_decimal(s);
}

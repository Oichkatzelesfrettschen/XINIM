/* Implementation of the standard abs() function. */

#include <stdlib.h>

/* Return the absolute value of the integer argument. */
int abs(int i) {
    /* Use a simple ternary expression to select the correct sign. */
    return (i < 0) ? -i : i;
}

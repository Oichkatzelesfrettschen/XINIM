/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Implementation of the standard abs() function. */

#include <stdlib.h>

/* Return the absolute value of the integer argument. */
int abs(int i) {
    /* Use a simple ternary expression to select the correct sign. */
    return (i < 0) ? -i : i;
}

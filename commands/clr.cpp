/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* clr - clear the screen		Author: Andy Tanenbaum */

int main(void) {
    /* Clear the screen. */

    prints("\033 8\033~0");
    exit(0);
}

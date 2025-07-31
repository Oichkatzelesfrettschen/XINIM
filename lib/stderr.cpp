/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include <unistd.h> // write system call

// Write a string to standard error.
void std_err(const char *s) {
    const char *p = s;
    while (*p != 0)
        p++;
    write(2, s, p - s);
}

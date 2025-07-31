/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Obtain information about the file named 'name'.
int stat(const char *name, char *buffer) {
    int n = callm1(FS, STAT, len(const_cast<char *>(name)), 0, 0,
                   const_cast<char *>(name), buffer, NIL_PTR);
    return n;
}

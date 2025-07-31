/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Create a new file with the specified mode. Returns a file descriptor on
// success or -1 on failure.
int creat(const char *name, int mode) {
    return callm3(FS, CREAT, mode, const_cast<char *>(name));
}

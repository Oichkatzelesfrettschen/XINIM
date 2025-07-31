/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Open the file 'name' with the given mode and return a descriptor.
int open(const char *name, int mode) {
    return callm3(FS, OPEN, mode, const_cast<char *>(name));
}

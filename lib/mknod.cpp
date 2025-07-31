/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Create a special file with the given mode and device address.
int mknod(const char *name, int mode, int addr) {
    return callm1(FS, MKNOD, len(const_cast<char *>(name)), mode, addr,
                  const_cast<char *>(name), NIL_PTR, NIL_PTR);
}

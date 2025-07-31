/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Unmount the file system mounted at 'name'.
int umount(const char *name) {
    return callm3(FS, UMOUNT, 0, const_cast<char *>(name));
}

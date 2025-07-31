/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Reposition the read/write file offset for descriptor 'fd'.
long lseek(int fd, long offset, int whence) {
    int k;
    M.m2_i1() = fd;
    M.m2_l1() = offset;
    M.m2_i2() = whence;
    k = callx(FS, LSEEK);
    if (k != OK)
        return ((long)k); /* send itself failed */
    return (M.m2_l1());
}

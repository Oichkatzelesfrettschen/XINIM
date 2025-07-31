/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Create an anonymous pipe and store the file descriptors into 'fild'.
int pipe(int fild[2]) {
    int k = callm1(FS, PIPE, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k >= 0) {
        fild[0] = M.m1_i1();
        fild[1] = M.m1_i2();
        return (0);
    } else
        return (k);
}

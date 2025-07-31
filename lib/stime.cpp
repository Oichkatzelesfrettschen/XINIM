/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Set the system time to the value pointed to by 'top'.
int stime(long *top) {
    M.m2_l1() = *top;
    return callx(FS, STIME);
}

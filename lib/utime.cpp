/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Update the access and modification times of 'name'.
int utime(const char *name, long timp[2]) {
    M.m2_i1() = len(const_cast<char *>(name));
    M.m2_l1() = timp[0];
    M.m2_l2() = timp[1];
    M.m2_p1() = const_cast<char *>(name);
    return callx(FS, UTIME);
}

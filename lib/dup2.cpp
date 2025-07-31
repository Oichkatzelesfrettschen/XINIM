/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Duplicate 'fd' to the descriptor 'fd2'. Returns the duplicated descriptor
// or -1 on failure.
int dup2(int fd, int fd2) {
    return callm1(FS, DUP, fd + 0100, fd2, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

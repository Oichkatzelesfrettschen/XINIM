/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Current end of the data segment managed by brk/sbrk.
extern char *brksize;

// Set the program break to the address specified by 'addr'.
char *brk(char *addr) {
    int k;

    k = callm1(MM, BRK, 0, 0, 0, addr, NIL_PTR, NIL_PTR);
    if (k == OK) {
        brksize = M.m2_p1();
        return (NIL_PTR);
    } else {
        return ((char *)-1);
    }
}

// Increment the program break by 'incr' bytes.
char *sbrk(int incr) {
    char *newsize, *oldsize;
    extern int endv, dorgv;

    oldsize = brksize;
    newsize = brksize + incr;
    if (brk(newsize) == 0)
        return (oldsize);
    else
        return ((char *)-1);
}

#include "../include/lib.hpp" // C++17 header

extern char *brksize;

// Set the program break to the address specified by 'addr'.
PUBLIC char *brk(char *addr) {
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
PUBLIC char *sbrk(int incr) {
    char *newsize, *oldsize;
    extern int endv, dorgv;

    oldsize = brksize;
    newsize = brksize + incr;
    if (brk(newsize) == 0)
        return (oldsize);
    else
        return ((char *)-1);
}

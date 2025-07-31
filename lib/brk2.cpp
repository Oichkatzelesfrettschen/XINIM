/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Forward declaration from getutil.cpp
extern phys_clicks get_size(void);

// Request memory segment move to the end of BSS.
void brk2() {
    char *p1, *p2;

    p1 = (char *)get_size();
    callm1(MM, BRK2, 0, 0, 0, p1, p2, NIL_PTR);
}

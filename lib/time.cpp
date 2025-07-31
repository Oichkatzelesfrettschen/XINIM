/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Return the current system time; optionally store it in '*tp'.
long time(long *tp) {
    int k;
    long l;
    k = callm1(FS, TIME, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (M.m_type < 0 || k != OK) {
        errno = -M.m_type;
        return (-1L);
    }
    l = M.m2_l1();
    if (tp != (long *)0)
        *tp = l;
    return (l);
}

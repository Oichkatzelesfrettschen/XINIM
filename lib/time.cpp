#include "../include/lib.hpp" // C++23 header

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

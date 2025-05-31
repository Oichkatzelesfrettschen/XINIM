#include "../include/lib.hpp" // C++17 header

// Set the system time to the value pointed to by 'top'.
int stime(long *top) {
    M.m2_l1() = *top;
    return callx(FS, STIME);
}

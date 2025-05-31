#include "../include/lib.hpp" // C++17 header

// Obtain the effective user ID of the calling process.
uid geteuid() {
    int k = callm1(MM, GETUID, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k < 0)
        return static_cast<uid>(k);
    return static_cast<uid>(M.m2_i1());
}

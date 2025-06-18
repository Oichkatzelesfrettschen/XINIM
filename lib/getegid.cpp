#include "../include/lib.hpp" // C++23 header

// Obtain the effective group ID of the calling process.
gid getegid() {
    int k = callm1(MM, GETGID, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k < 0)
        return static_cast<gid>(k);
    return static_cast<gid>(M.m2_i1());
}

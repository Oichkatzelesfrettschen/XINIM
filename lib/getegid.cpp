#include "../include/lib.hpp" // C++17 header

PUBLIC gid getegid() {
    int k;
    k = callm1(MM, static_cast<int>(SysCall::GETGID), 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k < 0)
        return ((gid)k);
    return ((gid)M.m2_i1);
}

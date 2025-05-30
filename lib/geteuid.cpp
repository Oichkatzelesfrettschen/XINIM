#include "../include/lib.hpp" // C++17 header

PUBLIC uid geteuid() {
    int k;
    k = callm1(MM, static_cast<int>(SysCall::GETUID), 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k < 0)
        return ((uid)k);
    return ((uid)M.m2_i1);
}

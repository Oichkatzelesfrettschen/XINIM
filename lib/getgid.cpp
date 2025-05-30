#include "../include/lib.hpp" // C++17 header

PUBLIC gid getgid() {
    int k;
    k = callm1(MM, static_cast<int>(SysCall::GETGID), 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    return ((gid)k);
}

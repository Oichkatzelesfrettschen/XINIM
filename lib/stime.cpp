#include "../include/lib.hpp" // C++17 header

PUBLIC int stime(top)
long *top;
{
    M.m2_l1 = *top;
    return callx(FS, static_cast<int>(SysCall::STIME));
}

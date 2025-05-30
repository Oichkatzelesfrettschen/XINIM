#include "../include/lib.hpp" // C++17 header

PUBLIC int link(name, name2)
char *name, *name2;
{
    return callm1(FS, static_cast<int>(SysCall::LINK), len(name), len(name2), 0, name, name2,
                  NIL_PTR);
}

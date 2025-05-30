#include "../include/lib.hpp" // C++17 header

PUBLIC int chown(name, owner, grp)
char *name;
int owner, grp;
{
    return callm1(FS, static_cast<int>(SysCall::CHOWN), len(name), owner, grp, name, NIL_PTR,
                  NIL_PTR);
}

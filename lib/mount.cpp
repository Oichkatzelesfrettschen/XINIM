#include "../include/lib.hpp" // C++17 header

PUBLIC int mount(special, name, rwflag)
char *name, *special;
int rwflag;
{
    return callm1(FS, static_cast<int>(SysCall::MOUNT), len(special), len(name), rwflag, special,
                  name, NIL_PTR);
}

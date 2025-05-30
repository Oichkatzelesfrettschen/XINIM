#include "../include/lib.hpp" // C++17 header

PUBLIC int open(name, mode)
char *name;
int mode;
{
    return callm3(FS, static_cast<int>(SysCall::OPEN), mode, name);
}

#include "../include/lib.hpp" // C++17 header

PUBLIC int chdir(name)
char *name;
{
    return callm3(FS, static_cast<int>(SysCall::CHDIR), 0, name);
}

#include "../include/lib.hpp" // C++17 header

PUBLIC int access(name, mode)
char *name;
int mode;
{
    return callm3(FS, ACCESS, mode, name);
}

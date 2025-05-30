#include "../include/lib.hpp" // C++17 header

PUBLIC int creat(name, mode)
char *name;
int mode;
{
    return callm3(FS, CREAT, mode, name);
}

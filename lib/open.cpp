#include "../include/lib.hpp" // C++17 header

int open(name, mode)
char *name;
int mode;
{
    return callm3(FS, OPEN, mode, name);
}

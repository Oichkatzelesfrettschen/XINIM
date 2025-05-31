#include "../include/lib.hpp" // C++17 header

int link(name, name2)
char *name, *name2;
{
    return callm1(FS, LINK, len(name), len(name2), 0, name, name2, NIL_PTR);
}

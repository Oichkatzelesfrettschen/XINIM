#include "../include/lib.hpp" // C++17 header

int mknod(name, mode, addr)
char *name;
int mode, addr;
{
    return callm1(FS, MKNOD, len(name), mode, addr, name, NIL_PTR, NIL_PTR);
}

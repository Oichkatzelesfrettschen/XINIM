#include "../include/lib.hpp" // C++17 header

int umount(name)
char *name;
{
    return callm3(FS, UMOUNT, 0, name);
}

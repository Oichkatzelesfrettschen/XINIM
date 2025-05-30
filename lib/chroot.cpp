#include "../include/lib.hpp" // C++17 header

PUBLIC int chroot(name)
char *name;
{
    return callm3(FS, CHROOT, 0, name);
}

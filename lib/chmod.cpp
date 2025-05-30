#include "../include/lib.hpp" // C++17 header

PUBLIC int chmod(name, mode)
char *name;
int mode;
{
    return callm3(FS, CHMOD, mode, name);
}

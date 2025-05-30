#include "../include/lib.hpp" // C++17 header

PUBLIC int chdir(name)
char *name;
{
    return callm3(FS, CHDIR, 0, name);
}

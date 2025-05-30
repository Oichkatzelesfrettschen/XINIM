#include "../include/lib.hpp" // C++17 header

PUBLIC int unlink(name)
char *name;
{
    return callm3(FS, UNLINK, 0, name);
}

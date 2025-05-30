#include "../include/lib.hpp" // C++17 header

PUBLIC int dup(fd)
int fd;
{
    return callm1(FS, DUP, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

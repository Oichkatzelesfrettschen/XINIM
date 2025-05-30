#include "../include/lib.hpp" // C++17 header

PUBLIC int close(fd)
int fd;
{
    return callm1(FS, static_cast<int>(SysCall::CLOSE), fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

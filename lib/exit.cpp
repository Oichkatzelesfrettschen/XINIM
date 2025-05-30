#include "../include/lib.hpp" // C++17 header

PUBLIC int exit(status)
int status;
{
    return callm1(MM, static_cast<int>(SysCall::EXIT), status, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

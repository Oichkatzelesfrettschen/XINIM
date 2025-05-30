#include "../include/lib.hpp" // C++17 header

PUBLIC int umask(complmode)
int complmode;
{
    return callm1(FS, UMASK, complmode, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

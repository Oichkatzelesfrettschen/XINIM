#include "../include/lib.hpp" // C++17 header

PUBLIC int alarm(sec)
unsigned sec;
{
    return callm1(MM, ALARM, (int)sec, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

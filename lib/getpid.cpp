#include "../include/lib.hpp" // C++17 header

PUBLIC int getpid() { return callm1(MM, GETPID, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

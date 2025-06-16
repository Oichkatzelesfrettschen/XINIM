#include "../include/lib.hpp" // C++23 header

// Return the process ID of the calling process.
int getpid() { return callm1(MM, GETPID, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

#include "../include/lib.hpp" // C++23 header

// Set the real and effective user ID for the process.
int setuid(int usr) { return callm1(MM, SETUID, usr, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

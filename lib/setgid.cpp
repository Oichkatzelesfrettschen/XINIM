#include "../include/lib.hpp" // C++23 header

// Set the real and effective group ID for the process.
int setgid(int grp) { return callm1(MM, SETGID, grp, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

#include "../include/lib.hpp" // C++17 header

// Set the group ID of the calling process.
PUBLIC int setgid(int grp) { return callm1(MM, SETGID, grp, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

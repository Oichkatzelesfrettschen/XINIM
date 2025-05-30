#include "../include/lib.hpp" // C++17 header

// Set the user ID of the calling process.
PUBLIC int setuid(int usr) { return callm1(MM, SETUID, usr, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

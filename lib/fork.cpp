#include "../include/lib.hpp" // C++17 header

// Create a new process duplicating the calling process.
int fork() { return callm1(MM, FORK, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

#include "../include/lib.hpp" // C++23 header

// Suspend the process until a signal is received.
int pause() { return callm1(MM, PAUSE, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

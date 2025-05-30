#include "../include/lib.hpp" // C++17 header

PUBLIC int sync() { return callm1(FS, SYNC, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

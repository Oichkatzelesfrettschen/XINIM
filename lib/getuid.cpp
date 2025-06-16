#include "../include/lib.hpp" // C++23 header

// Obtain the real user ID of the calling process.
uid getuid() {
    int k = callm1(MM, GETUID, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    return static_cast<uid>(k);
}

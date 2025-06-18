#include "../include/lib.hpp" // C++23 header

// Obtain the real group ID of the calling process.
gid getgid() {
    int k = callm1(MM, GETGID, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    return static_cast<gid>(k);
}

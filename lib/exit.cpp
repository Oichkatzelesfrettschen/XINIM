#include "../include/lib.hpp" // C++17 header

// Terminate the calling process with the given status code.
int exit(int status) {
    return callm1(MM, EXIT, status, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

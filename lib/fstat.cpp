#include "../include/lib.hpp" // C++17 header

// Obtain file status for an open descriptor.
PUBLIC int fstat(int fd, char *buffer) {
    int n;
    n = callm1(FS, FSTAT, fd, 0, 0, buffer, NIL_PTR, NIL_PTR);
    return n;
}

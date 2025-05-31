#include "../include/lib.hpp" // C++17 header

// Get status information for the open file descriptor 'fd'.
int fstat(int fd, char *buffer) {
    int n = callm1(FS, FSTAT, fd, 0, 0, buffer, NIL_PTR, NIL_PTR);
    return n;
}

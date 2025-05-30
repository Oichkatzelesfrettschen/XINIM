#include "../include/lib.hpp" // C++17 header

// Read from a file descriptor into 'buffer'.
PUBLIC int read(int fd, char *buffer, int nbytes) {
    int n;
    n = callm1(FS, READ, fd, nbytes, 0, buffer, NIL_PTR, NIL_PTR);
    return n;
}
